#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "queue.h"
#include "ext_array.h"
#include "window.h"
#include "utils.h"

#define QUEUE_CAPACITY 1024

typedef enum status{
	NOT_WAITING,
	WAITING // on an ack	
} status_e;

struct timed_chunk{
	time_t start_time;
	int left; 
	int right;
	status_e status; 
};

typedef struct timed_chunk* timed_chunk_t;

timed_chunk_t timed_chunk_init(int left, int right){
	timed_chunk_t tc = malloc(sizeof(struct timed_chunk));
	
	tc->left = left; tc->right = right;
	tc->status = NOT_WAITING;

	return tc;
}

///////////// WINDOW CHUNK ////////////

/* gets data from left all the way up to (but NOT including) right */
window_chunk_t window_chunk_init(void* wrap, int length, int left, int right){
	window_chunk_t window_chunk = malloc(sizeof(struct window_chunk));
	
	if(right > left) {
		window_chunk->data = malloc(right-left);
		memcpy(window_chunk->data, wrap+left, right-left);
	}
	else {
		window_chunk->data = malloc((length - left) + right);
		memcpy(window_chunk->data, wrap+left, length-left);
		memcpy(window_chunk->data+(length-left), wrap+0, right);
	}

	window_chunk->seqnum = (right - 1) % length;
	window_chunk->length = WRAP_DIFF(left, right, length);
	
	return window_chunk;
}

void window_chunk_destroy(window_chunk_t* wc){
	free(*wc);
	*wc = NULL;
}

void window_chunk_destroy_total(window_chunk_t* wc, destructor_f destructor){
	destructor(&((*wc)->data));
	window_chunk_destroy(wc);
}	

void window_chunk_destroy_free(window_chunk_t* wc){
	free((*wc)->data);
	window_chunk_destroy(wc);
}

///////////// WINDOW //////////////////

struct window{
	ext_array_t data_queue;
	queue_t to_send;
	int sent_left;	
	
	double timeout;
	int size;
	int send_size;
	void* slider;

	int left;
	int right;

	timed_chunk_t* timed_chunks;
};

window_t window_init(double timeout, int window_size, int send_size){
	window_t window = (window_t)malloc(sizeof(struct window));

	window->data_queue = ext_array_init(QUEUE_CAPACITY);
	window->to_send    = queue_init();

	window->timeout 	= timeout;
	window->send_size   = send_size;
	window->size 		= window_size;
	window->slider  	= malloc(window_size*2);
	memset(window->slider, 0, window_size*2);

	window->timed_chunks = malloc(sizeof(timed_chunk_t)*2*window_size);
	memset(window->timed_chunks, 0, sizeof(timed_chunk_t)*2*window_size);

	window->left = window->right = window->sent_left = 0;

	return window;
}

void window_destroy(window_t* window){
	ext_array_destroy(&((*window)->data_queue));
	
	timed_chunk_t chunk;
	if((*window)->size*2 > 0){
		chunk = (*window)->timed_chunks[0];

		int until;
		if (chunk)
			until = chunk->left == 0 ? (*window)->size*2 : chunk->left;
		else 
			until = (*window)->size*2;

		int i;
		for(i=0;i<until;i++){
			chunk = (*window)->timed_chunks[i];
			if(chunk){
				i = chunk->right + 1;
				free(chunk);
			}
		}	
	}
	
	queue_destroy_total(&((*window)->to_send), (destructor_f)window_chunk_destroy_free);
	free((*window)->slider);
	free((*window)->timed_chunks);
	free(*window);
	*window = NULL;
}

/* copying the data, so you can throw it out */
void window_push(window_t window, void* data, int length){
	printf("STARTING PUSH: length - %d\n", length);
	window_print(window);
	puts("");

	if (WRAP_DIFF(window->left, window->right, window->size*2) == window->size) { 
		ext_array_push(window->data_queue, data, length);
	}

	else {
		int left_in_window = WRAP_DIFF(window->right, WRAP_ADD(window->left, window->size, window->size*2), window->size*2);

		/* we're only gonna fill it up until its full, so take the minimum of what's left in the
			window, and the length */

		int fill_window = MIN(left_in_window, length);

		/* only fill it up until you fall of the array on the right, then wrap around */
		int fill_right = window->right >= window->left ? MIN(window->size*2 - window->right, fill_window) : fill_window;

		/* copy in to the right */
		memcpy(window->slider+window->right, data, fill_right);
		
		/* if it overflows, wrap around and reset window->right to account for the new data */
		if(fill_window - fill_right) {
			memcpy(window->slider, data, fill_window - fill_right); 
			window->right = fill_window - fill_right;
		}
		else {
			window->right = window->right + fill_right;
		}

		/* and if it all couldn't fit in the window, push the rest of it to the data queue */
		if(length - fill_window)
			ext_array_push(window->data_queue, data+fill_window, length - fill_window);
	}

	window_print(window);
	puts("DONE\n");
}

void window_ack(window_t window, int seqnum){
	printf("ACKING %d\n", seqnum);
	window_print(window);

	if (seqnum < 0 || seqnum > window->size*2){
		LOG(("Trying to ack with invalid seqnum. Should be 0 <= seqnum < %d, given: %d", window->size*2, seqnum));
		return;
	}

	int last_received = seqnum == 0 ? window->size*2 : seqnum - 1;

	timed_chunk_t acked_chunk = window->timed_chunks[last_received];
	if(!acked_chunk)
		{ LOG(("Acked chunk is null at seqnum %d", seqnum)); return; }

	/* if the chunk was the left-most chunk in the window, ie we were waiting on this chunk in order
		to slideee the window over, then find a new left that is not null, but isn't the right
		(otherwise that's infinite loop business) */

	if (window->left == acked_chunk->left){

		/* might as well start at seqnum because we know at least up to seqnum has been acked */
		int new_left = seqnum;
		while( new_left != window->right && window->timed_chunks[new_left] == NULL ) 
			new_left = (new_left + 1) % (window->size*2); 

		/* ok so the new_left now points to the left-most byte that HAS NOT been acked, ie points to a non-NULL chunk */
		window->left = new_left;
	
		memchunk_t stuff_window = ext_array_peel(window->data_queue, WRAP_DIFF(window->right, WRAP_ADD(window->left, window->size, window->size*2), window->size*2));
		if(stuff_window) {

			int stuff_window_size = stuff_window->length;
			int stuff_window_right = window->right > window->left ? MIN(window->size*2 - window->right, stuff_window_size) : stuff_window_size;

			memcpy(window->slider+window->right, stuff_window->data, stuff_window_right);
			if(stuff_window_size - stuff_window_right) {
				memcpy(window->slider+0, stuff_window->data+stuff_window_right, stuff_window_size - stuff_window_right);
				window->right = stuff_window_size - stuff_window_right;
			}
			else {
				window->right = window->right + stuff_window_right;
			}
			
			memchunk_destroy_total(&stuff_window, util_free);
		}	
	}

	int chunk_left = acked_chunk->left;
	int transmitted = WRAP_DIFF(acked_chunk->left, last_received, window->size*2);

	if(last_received == acked_chunk->right) 
		free(acked_chunk);

	else
		acked_chunk->left = seqnum;		

	/* all the timed_chunk pointers that are pointing to the chunk of memory that the acked_chunk
		points to can be set to NULL so that we know that they've been ACKED */
	int i;
	for(i=chunk_left; i!=seqnum; i = (i+1) % (window->size*2)) 
		window->timed_chunks[i] = NULL;	
	
	puts("");
	window_print(window);
	printf("DONE");

	return;
}

/* Will handle going through all the timers and for the ones who have 
   timers that have elapsed time > timeout, adds these to the to_send
   queue and resets the timer. */
void window_check_timers(window_t window){
	timed_chunk_t timed_chunk = NULL;

	/* get the time */
	time_t now;
	time(&now);

	int i;
	for(i=0;i<2*window->size;){

		/* find the next timed_chunk that isn't the current one */
		while( i<2*window->size && window->timed_chunks[i] == timed_chunk) i++;
		timed_chunk = window->timed_chunks[i];

		/* if the new chunk is actually a chunk, then check if it's waiting for an ack,
			and if it is then check the time-elapsed. If more than window->timeout, then
			push it onto the front of the to_send queue, and set its status to NOT_WAITING
			(because it hasn't been sent yet) */
		if(timed_chunk && 
				timed_chunk->status == WAITING && 
					difftime(now, timed_chunk->start_time) > window->timeout) {
	
			queue_push_front(window->to_send, (void*)timed_chunk);
			timed_chunk->status = NOT_WAITING;
		}
	}
}	
	
/* when you call window_get_next, the window pulls data from the data_queue and
	stuffs it into the window at the appropriate location. */
window_chunk_t window_get_next(window_t window){
	timed_chunk_t chunk = (timed_chunk_t)queue_pop(window->to_send);
	if (!chunk)	{
		int new_sent_left;
		int unsent_in_window = WRAP_DIFF(window->sent_left, window->right, window->size*2);
		if (!unsent_in_window)
			return NULL; // nothing to send (waiting on everything)

		new_sent_left = (window->sent_left + MIN(unsent_in_window, window->send_size)) % (window->size*2);

		chunk = timed_chunk_init(window->sent_left, new_sent_left);
		// now set all the bytes that are covered by this chunk to point to it
		int i; 
		for(i=window->sent_left; i!= new_sent_left; i = (i+1) % (window->size*2))
			window->timed_chunks[i] = chunk;	

		window->sent_left = new_sent_left;
	}
		
	time(&(chunk->start_time));
	chunk->status = WAITING;	
	return window_chunk_init(window->slider, window->size*2, chunk->left, chunk->right);
}
		

void window_print(window_t window){
	printf("Left: %d\nRight: %d\nSent_left: %d\n", window->left, window->right, window->sent_left);
}


