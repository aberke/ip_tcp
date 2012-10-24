#include <stdlib.h>
#include <time.h>

#include "queue.h"
#include "window.h"
#include "utils.h"

#define QUEUE_CAPACITY 1024

///////////// TIMER //////////////////////

typedef enum timer_status{
	NOTSTARTED=0,
	STARTED,
	ENDED
} timer_status_e;

struct timed_chunk{
	time_t start_time;
	timer_status_e status;
	memchunk_t chunk;
	int index;
};

typedef struct timed_chunk* timed_chunk_t;

timed_chunk_t timed_chunk_init(){
	timed_chunk_t timed_chunk = malloc(sizeof(struct timed_chunk));
	timed_chunk->status = NOTSTARTED;
	timed_chunk->chunk = NULL;
	timed_chunk->index = -1;

	return timed_chunk;
}

void timed_chunk_destroy(timed_chunk_t* timed_chunk){
	if((*timed_chunk)->chunk)
		memchunk_destroy(&((*timed_chunk)->chunk));

	free(*timed_chunk);
	*timed_chunk = NULL;
}

void timed_chunk_reset(timed_chunk_t timed_chunk, memchunk_t chunk, int index){
	timed_chunk->status = NOTSTARTED;
	timed_chunk->chunk = chunk;
	timed_chunk->index = index;
}

void timed_chunk_restart_timer(timed_chunk_t timed_chunk){
	if(timed_chunk->status != STARTED){
		LOG(("Restarting a timer that hasn't been started (or has already ended). This shouldn't happen (i don't think)"));
		timed_chunk->status = STARTED;
	}
	
	time(&(timed_chunk->start_time));
}
	

void timed_chunk_start(timed_chunk_t timed_chunk){
	if(timed_chunk->status == NOTSTARTED){
		time(&(timed_chunk->start_time));
		timed_chunk->status = STARTED;
	}
	else
		LOG(("Unable to start timed_chunk that has already been started"));
}

boolean timed_chunk_started(timed_chunk_t timed_chunk){
	return !(timed_chunk->status == NOTSTARTED);
}

boolean timed_chunk_ended(timed_chunk_t timed_chunk){
	return timed_chunk->status == ENDED;
}

void timed_chunk_stop(timed_chunk_t timed_chunk){
	if(timed_chunk->status == STARTED)
		timed_chunk->status = ENDED;
	
	else
		LOG(("Unable to end timed_chunk that does not have status equal to STARTED"));
}

double timed_chunk_elapsed(timed_chunk_t timed_chunk){
	time_t now;
	time(&now);
	return difftime(now, timed_chunk->start_time);
}

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
	
	double timeout;
	int size;

	timed_chunk_t* timed_chunks;
};

window_t window_init(double timeout, int window_size){
	window_t window = (window_t)malloc(sizeof(struct window));

	window->data_queue = ext_array_init(QUEUE_CAPACITY);

	window->timeout 	= timeout;
	window->size 		= window_size;

	window->timed_chunks = malloc(sizeof(timed_chunk_t)*2*window_size);
	memset(window->timed_chunks, 0, sizeof(timed_chunk_t)*2*window_size);

	return window;
}

void window_destroy(window_t* window){
	ext_array_destroy(&((*window)->data_queue));
	
	int i;
	for(i=0;i<2*window->size;i++){
		if((*window)->timed_chunks[i]) 
			timed_chunk_destroy(&((*window)->timed_chunks[i]));
	}
	free((*window)->timed_chunks);

	free(*(window));
	*window = NULL;
}

/* copying the data in chunk, so you can throw it out */
void window_push(window_t window, memchunk_t chunk){
	ext_array_push(window->data_queue, chunk->data, chunk->length);
}

void window_ack(window_t window, int seqnum){
	if (seqnum < 0 || seqnum > window->size*2){
		LOG(("Trying to ack with invalid seqnum. Should be 0 <= seqnum < %d, given: %d", window->size*2, seqnum));
		return;
	}

	timed_chunk_t acked_chunk = window->timed_chunks[seqnum];
	if(!acked_chunk)
		{ LOG(("Acked chunk is null")); return; }

	

	

	if(seqnum < 0 || seqnum >= window->size*2){
		LOG(("Trying to ack with invalid seqnum. Should be 0 <= seqnum < %d, given: %d", window->size*2, seqnum));
		return;
	}

	/* if the timed_chunk isn't started (ie it's ended already or it hasn't been 
	   started yet, this is an error */
	if(!timed_chunk_started(window->timed_chunks[seqnum])){
		puts("BLAHD BLAH BLAH");
		LOG(("Trying to ack when timed_chunk hasn't been started for seqnum %d", seqnum));	
		return;
	}
	else{
		/* stop the timed_chunk for the acked position */
		timed_chunk_stop(window->timed_chunks[seqnum]);

		/* you've received an ack for this piece of data, so you will
           never need it again. ie, destroy it. But keep the timed
		   chunk around, per usual */
		timed_chunk_t timed_chunk = window->timed_chunks[seqnum];
		memchunk_t chunk;
		void* data;

		chunk = timed_chunk->chunk;
		data = chunk->data;

		memchunk_destroy(&chunk);
	
		/* now if the ack you received is for the left side of the window, 
		   slide the window over until you find an unacked spot, as you slide
		   over, attempt to fill the window with queued data. */
		if(seqnum == window->left){
			int new_left = seqnum;
			int new_right = window->right;
			
			/* while the left side of the window has ended its timed_chunk (ie an 
			   ack has been received), reset that timed_chunk. Then shift the left
			   over, and try to add some more to the right side of the window. */ 
			while(timed_chunk_ended(window->timed_chunks[new_left])){
				timed_chunk_reset(window->timed_chunks[new_left], NULL, -1);
				new_left = (new_left + 1) % window->size;			

				chunk = (memchunk_t)queue_pop(window->data_queue);
				if(chunk){
					/* If there's stuff on the data queue, then push it on
					   to the right side of the window, and move the right side
					   over accordingly, also reset the timed_chunk for that chunk 
					   (put it in the UNSTARTED phase). */
					new_right = (new_right + 1) % window->size;

					timed_chunk = window->timed_chunks[new_right];
					timed_chunk_reset(timed_chunk, chunk, new_right);
					queue_push(window->to_send, (void*)timed_chunk);
				}	
			}
			window->left  = new_left;
			window->right = new_right;
		}
	}
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
	
/* Once you pull it out, the timer starts */
window_chunk_t window_get_next(window_t window){
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
		


