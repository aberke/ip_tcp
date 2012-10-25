#include <stdlib.h>
#include <time.h>

#include "queue.h"
#include "window.h"
#include "utils.h"

#define QUEUE_CAPACITY 1024
#define ACKED NULL

struct timed_chunk{
	time_t start_time;
	int left; 
	int right;
};

typedef struct timed_chunk* timed_chunk_t;

///////////// WINDOW CHUNK ////////////
static window_chunk_t window_chunk_init(timed_chunk_t timed_chunk){
	window_chunk_t wc = malloc(sizeof(struct window_chunk));
	wc->chunk = timed_chunk->chunk;
	wc->seqnum = timed_chunk->index;
	return wc;
}

void window_chunk_destroy(window_chunk_t* wc){
	free(*wc);
	*wc = NULL;
}

///////////// WINDOW //////////////////

struct window{
	ext_array_t data_queue;
	queue_t to_send;
	
	double timeout;
	int size;
	void* slider;

	int left;
	int right;

	timed_chunk_t* timed_chunks;
};

window_t window_init(double timeout, int window_size){
	window_t window = (window_t)malloc(sizeof(struct window));

	window->data_queue = ext_array_init(QUEUE_CAPACITY);

	window->timeout 	= timeout;
	window->size 		= window_size;
	window->slider  	= window_size*2;

	window->timed_chunks = malloc(sizeof(timed_chunk_t)*2*window_size);
	memset(window->timed_chunks, 0, sizeof(timed_chunk_t)*2*window_size);

	window->left = window->right = 0;

	return window;
}

void window_destroy(window_t* window){
	ext_array_destroy(&((*window)->data_queue));
	
	int i;
	for(i=0;i<2*window->size;i++){
		if((*window)->timed_chunks[i]) 
			free((*window)->timed_chunks[i]);
	}

	free((*window)->timed_chunks);
	free(*window);
	*window = NULL;
}

/* copying the data, so you can throw it out */
void window_push(window_t window, void* data, int length){
	if (WRAP_DIFF(window->left, window->right, window->size*2) == window->size) { 
		ext_array_push(window->data_queue, data, length)
	}

	else {
		/* we're only gonna fill it up until its full, so take the minimum of what's left in the
			window, and the length */
		int fill_window = MIN(WRAP_DIFF(window->right, window->left, window->size*2), length);

		/* only fill it up until you fall of the array on the right, then wrap around */
		int fill_right = MIN(window->size*2 - window->left, fill_window);

		/* copy in to the right */
		memcpy(window->slider+window->left, data, fill_right);
		
		/* if it overflows, wrap around */
		if(fill_window - fill_right) {
			memcpy(window->slider, data, fill_window - fill_right); 
			window->right = fill_window - fill_right;
		}
		else {
			window->right = window->left

		/* and if it all couldn't fit in the window, push the rest of it to the data queue */
		if(length - fill_window)
			ext_array_push(window->data_queue, data, length - fill_window);
	}
}

void window_ack(window_t window, int seqnum){
	if (seqnum < 0 || seqnum > window->size*2){
		LOG(("Trying to ack with invalid seqnum. Should be 0 <= seqnum < %d, given: %d", window->size*2, seqnum));
		return;
	}

	timed_chunk_t acked_chunk = window->timed_chunks[seqnum];
	if(!acked_chunk)
		{ LOG(("Acked chunk is null")); return; }

	/* acked_chunk exists. so all the data from the left side of that chunk
		all the way up to but not including the acked number has been successfully 
		transmitted, but no more. Either way, we can free up (however we want to do
		that) the data from chunk->left to seqnum-1, and then change the timed_chunk
		so it now only points to the non-sent data (if none left, then just destroy it).
	*/

	int upto = seqnum == 0 ? window->size*2 : seqnum - 1;
	int transmitted = acked_chunk->left > upto ?  // this is the case if the chunk wrapped around
								window->size*2 - acked_chunk->left + upto :
								upto - acked_chunk->left;

	/* all the timed_chunk pointers that are pointing to the chunk of memory that the acked_chunk
		points to can be set to ACKED so that we know that they've been ACKED */
	int i;
	for(i=0; i<transmitted; i = (i+1) % window->size*2){
		window->timed_chunks[i] = ACKED;	
	
	/* if all the data in the current chunk has been transmitted, we don't need it anymore */
	if(upto == acked_chunk->right)
		free(acked_chunk);

	else
		acked_chunk->left = upto;

	return;
}

/* Will handle going through all the timers and for the ones who have 
   timers that have elapsed time > timeout, adds these to the to_send
   queue and resets the timer. */
void window_check_timers(window_t window){
	double time_elapsed;
	timed_chunk_t timed_chunk;
	
	int i;
	for(i=0;i<2*window->size;i++){
		timed_chunk = window->timed_chunks[i];
		if(timed_chunk->status != STARTED) continue;

		time_elapsed = timed_chunk_elapsed(timed_chunk);
		if( time_elapsed > window->timeout ){
			/* If the time elapsed is more than the allowable timeout, then
			   push it onto the queue to send, and restart the timer */
			queue_push(window->to_send, timed_chunk);	
			timed_chunk_restart_timer(timed_chunk);	
		}
	}
}	
	
/* when you call window_get_next, the window pulls data from the data_queue and
	stuffs it into the window at the appropriate location. */
window_chunk_t window_get_next(window_t window){
	void* chunk = queue_pop(window->to_send);
	if (!chunk) 
		return NULL; 

	else {
			

	timed_chunk_t timed_chunk = (timed_chunk_t)queue_pop(window->to_send);
	if(timed_chunk){
		window_chunk_t window_chunk = window_chunk_init(timed_chunk);

		timed_chunk_start(timed_chunk);	
		return window_chunk;
	}
	else{
		return NULL;
	}
}
		


