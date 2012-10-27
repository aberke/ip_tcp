#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "queue.h"
#include "ext_array.h"
#include "window.h"
#include "utils.h"

#define QUEUE_CAPACITY 1024

#define wrap_src_memcpy(dst, src, src_offset, size, modulo)	\
do{															\
	int write_right = MIN((modulo) - (src_offset), size);	\
	memcpy(dst, (src)+(src_offset), write_right);			\
	memcpy((dst)+write_right, (src)+0, (size)-write_right);	\
} 															\
while(0)
/*
#define wrap_dst_memcpy(dst, dst_offset, src, size, modulo) \
do{															\
	int write_right = MIN((modulo) - (dst_offset), size);	\
	memcpy((dst)+(dst_offset), (src), write_right);			\
	memcpy((dst)+0, (src)+write_right, (size)-write_right);	\
} 															\
while(0)
*/

#define window_memcpy(win, src, length) 						\
do{																\
	int write_right = MIN(((win)->size+1) - (((win)->right)%((win)->size+1)), length);\
	memcpy(((win)->slider)+(((win)->right)%((win)->size+1)), 	\
			(src), write_right);								\
	memcpy(((win)->slider)+0, (src)+write_right, (length)-write_right);\
} 																\
while(0)

#define window_get_seqnum(window, offset)\
	((window->wrap_count)*((window->size)+1)+(offset)) % MAX_SEQNUM 

typedef struct timed_chunk* timed_chunk_t;

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
	int wrap_count;
	int ISN;

	timed_chunk_t* timed_chunks;
	timed_chunk_t acked_chunk_placeholder;
};

/////////////// AUXILIARY DATA STRUCTURES /////////////////////////

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

timed_chunk_t timed_chunk_init(int left, int right){
	timed_chunk_t tc = malloc(sizeof(struct timed_chunk));
	
	tc->left = left; 
	tc->right = right;
	tc->status = NOT_WAITING;

	return tc;
}

///////////// WINDOW CHUNK ////////////

/* gets data from left all the way up to (but NOT including) right */
window_chunk_t window_chunk_init(window_t window, int left, int right){
	window_chunk_t window_chunk = malloc(sizeof(struct window_chunk));
	int wrap_length = WRAP_DIFF(left, right, MAX_SEQNUM);
	window_chunk->data = malloc(wrap_length);
	wrap_src_memcpy(window_chunk->data, window->slider, (left%(window->size+1)), wrap_length, window->size+1);

	window_chunk->seqnum = window_get_seqnum(window, left);
	window_chunk->length = wrap_length;
	
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


/// internal helpers ////

void _free_timers(window_t* window);

/////////////////////////

window_t window_init(double timeout, int window_size, int send_size, int ISN){
	window_t window = (window_t)malloc(sizeof(struct window));

	window->data_queue = ext_array_init(QUEUE_CAPACITY);
	window->to_send    = queue_init();

	window->timeout 	= timeout;
	window->send_size   = send_size;
	window->size 		= window_size;
	window->slider  	= malloc(window_size+1);
	memset(window->slider, 0, window_size);

	window->timed_chunks = malloc(sizeof(timed_chunk_t)*(window_size+1));
	memset(window->timed_chunks, 0, sizeof(timed_chunk_t)*(window_size+1));
	
	/* this will be a placeholder for when we have acked a time_chunk, but it
		isn't on the left of the window */
	window->acked_chunk_placeholder = timed_chunk_init(0, 0);

	window->left = window->right = window->sent_left = ISN;
	window->wrap_count = 0;
	window->ISN = ISN;

	return window;
}

void window_destroy(window_t* window){
	ext_array_destroy(&((*window)->data_queue));

	_free_timers(window);

	queue_destroy_total(&((*window)->to_send), (destructor_f)window_chunk_destroy_free);
	free((*window)->acked_chunk_placeholder);
	free((*window)->slider);
	free((*window)->timed_chunks);
	free(*window);
	*window = NULL;
}

void window_push(window_t window, void* data, int length){

	int left_in_window = window->size - WRAP_DIFF(window->left, window->right, MAX_SEQNUM);
	int to_write       = MIN(left_in_window, length);
	window_memcpy(window, data, to_write);
	ext_array_push(window->data_queue, data+to_write, length-to_write);
	window->right = (window->right + to_write) % MAX_SEQNUM;
	
}

window_chunk_t window_get_next(window_t window){
	timed_chunk_t chunk;
	if((chunk = (timed_chunk_t)queue_pop(window->to_send)) == NULL){
		int left_to_send = WRAP_DIFF(window->sent_left, window->right, MAX_SEQNUM);
		if(!left_to_send) 
			return NULL;
		else 
		{
			int chunk_size = MIN(window->send_size, left_to_send);
			chunk 	  	   = timed_chunk_init(window->sent_left, (window->sent_left + chunk_size) % MAX_SEQNUM);

			int i;
			for(i=0;i<chunk_size;i++){
				window->timed_chunks[(window->sent_left + i) % (window->size + 1)] = chunk;
			}

			window->sent_left = (window->sent_left + chunk_size) % MAX_SEQNUM;
		}
	}

	time(&(chunk->start_time));
	chunk->status = WAITING;

	return window_chunk_init(window, chunk->left, chunk->right);
}

void window_ack(window_t window, int seqnum){

	int window_min = window->left,
		window_max = window->right;
	
	if(!BETWEEN(seqnum, window_min, window_max))
		{ LOG(("Received invalid seqnum: %d, current window_min: %d, window_max: %d\n", seqnum, window_min, window_max)); return; }

	seqnum = seqnum==0 ? MAX_SEQNUM  : seqnum-1;
	
	timed_chunk_t acked_chunk = window->timed_chunks[(seqnum)%(window->size+1)];
	if(!acked_chunk)
		{ LOG(("NULL acked_chunk")); return; }


	int acked_left = acked_chunk->left;
	if (window->left == acked_left) {
		int acked_right = seqnum;
		do{ 
			acked_right = (acked_right+1) % MAX_SEQNUM;
			if(CONGRUENT(acked_left, acked_right, window->size+1)) break;
		}
		while(window->timed_chunks[(acked_right % (window->size+1))] == window->acked_chunk_placeholder);

		int i;
		for( i=(acked_left%(window->size+1));
				!CONGRUENT(i, acked_right, window->size+1);
					i=(i+1)%(window->size+1)) {
			window->timed_chunks[i] = NULL;
		}
	
		free(acked_chunk);

		window->left = acked_right;
		memchunk_t from_queue = ext_array_peel(window->data_queue, window->size-WRAP_DIFF(window->left, window->right, MAX_SEQNUM));
		if(from_queue){
			window_memcpy(window, from_queue->data, from_queue->length);
			window->right = (window->right + from_queue->length) % MAX_SEQNUM;
			memchunk_destroy_total(&from_queue, util_free);
		}
	}
	else{
		int i;
		for(i=(acked_left%(window->size+1));
			!CONGRUENT(i,seqnum,(window->size+1));
				i=(i+1)%(window->size+1)) 
			window->timed_chunks[i] = window->acked_chunk_placeholder;
		acked_chunk->left=seqnum;
	}
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
	for(i=0;i<(window->size+1);){

		/* find the next timed_chunk that isn't the current one */
		while( i<(window->size+1) && window->timed_chunks[i] == timed_chunk) i++;
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

void _free_timers(window_t* window){
	
	timed_chunk_t chunk;
	if((*window)->size > 0){
		chunk = (*window)->timed_chunks[0];

		int until;
		if (chunk)
			until = chunk->left == 0 ? (*window)->size : chunk->left%((*window)->size+1);
		else 
			until = (*window)->size;

		int i;
		for(i=0;i<until;){
			chunk = (*window)->timed_chunks[i];
			if(chunk && chunk != (*window)->acked_chunk_placeholder){
				i = chunk->right;
				free(chunk);
				if(i==0) break;
			}
			else i++;
		}	
	}
}
	
void window_print(window_t window){
	printf("Left: %d\nRight: %d\nSent_left: %d\n", window->left, window->right, window->sent_left);
}


