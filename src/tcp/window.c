#include <stdlib.h>
#include <time.h>

#include "queue.h"
#include "window.h"
#include "utils.h"

///////////// TIMER //////////////////////

typedef enum timer_status{
	NOTSTARTED=0,
	STARTED,
	ENDED
} timer_status_e;

struct timed_chunk{
	struct timeval start_time;
	timer_status_e status;
	memchunk_t chunk;
};

typedef struct timed_chunk* timed_chunk_t;

timed_chunk_t timed_chunk_init(memchunk_t chunk){
	timed_chunk_t timed_chunk = malloc(sizeof(struct timed_chunk));
	timed_chunk->status = NOTSTARTED;
	timed_chunk->chunk = chunk;

	return timed_chunk;
}

void timed_chunk_destroy(timed_chunk_t* timed_chunk){
	memchunk_destroy(&((*timed_chunk)->chunk));
	free(*timed_chunk);
	*timed_chunk = NULL;
}

void timed_chunk_reset(timed_chunk_t timed_chunk, memchunk_t chunk){
	timed_chunk->status = NOTSTARTED;
	timed_chunk->chunk = chunk;
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
	return !(timed_chunk->status != NOTSTARTED);
}

boolean timed_chunk_ended(timed_chunk_t timed_chunk){
	return timed_chunk->status == ENDED;
}

void timed_chunk_stop(timed_chunk_t timed_chunk){
	if(timed_chunk->status != STARTED){
		timed_chunk->status = ENDED;
	else
		LOG(("Unable to end timed_chunk that does not have status equal to STARTED"));
}

double timed_chunk_elapsed(timed_chunk_t timed_chunk){
	struct timeval now;
	time(&now);
	return difftime(now, timed_chunk->start_time);
}

///////////// WINDOW //////////////////

struct window{
	queue_t data_queue;
	queue_t to_send;	
	destructor_f destructor;
	
	double timeout;
	int size;
	int left;
	int right;

	timed_chunk_t* timed_chunks;
};

window_t window_init(double timeout, int window_size, destructor_f destructor){
	window_t window = (struct window*)malloc(sizeof(struct window));

	window->data_queue  = queue_init();
	window->to_send     = queue_init();
	window->destructor  = destructor;
	window->timeout 	= timeout;
	window->size 		= window_size;
	window->left 	    = 0;
	window->right 		= 0;

	window->timed_chunks   = (timed_chunk_t)malloc(sizeof(timed_chunk_t)*2*window_size);

	int i;
	for(i=0;i<2*window->size;i++){
		window->timed_chunks = timed_chunk_init(NULL);
	}
	
	return window;
}

void window_destroy(window_t* window){
	queue_destroy(&((*window)->data_queue));
	queue_destroy(&((*window)->to_send));
	
	for(int i=0;i<(*window)->window_size*2;i++){
		timed_chunk_destroy(&((*window)->timed_chunks[i]));
	}
	
	free((*window)->timed_chunks);
	free(*(window));
	*window = NULL;
}

void window_push(window_t window, memchunk_t chunk){
	// check if the window is full
	if( window->right > window->left && window->right - window->left >= window->size 
	 	|| window->left - window->right <= window->size )
	{
		queue_push(window->data_queue, (void*)chunk);		
	}
	else
	{
		int tofill = window->right;

		/* fill the sliding window position with the chunk, and 
		   also push it onto the queue to send, and set its timed_chunk
		   to UNSTARTED (it will start once its pulled from the 
		   to_send queue) */
		timed_chunk_reset(window->timed_chunks[tofill], chunk);
		queue_push(window->to_send, (void*)(window->timed_chunks[tofill]));

		/* shift the right over (and wrap around) */
		right = (right + 1) % window->size;
	}
}

void window_ack(window_t window, int seqnum){
	if(seqnum < 0 || seqnum >= window->size*2){
		LOG(("Trying to ack with invalid seqnum. Should be 0 <= seqnum < %d, given: %d", window->size*2, seqnum));
		return;
	}

	/* if the timed_chunk isn't started (ie it's ended already or it hasn't been 
	   started yet, this is an error */
	if(!timed_chunk_started(window->timed_chunks[seqnum])){
		LOG(("Trying to ack when timed_chunk hasn't been started for seqnum %d", seqnum));	
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
		window->destructor(&data);

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
				timed_chunk_reset(window->timed_chunks[new_left], NULL);
				new_left = (new_left + 1) % window->size;			

				chunk = (memchunk_t)queue_pop(window->data_queue);
				if(chunk){
					/* If there's stuff on the data queue, then push it on
					   to the right side of the window, and move the right side
					   over accordingly, also reset the timed_chunk for that chunk 
					   (put it in the UNSTARTED phase). */
					new_right = (new_right + 1) % window->size;

					timed_chunk = window->timed_chunk[new_right];
					timed_chunk_reset(timed_chunk, chunk);
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
	double timeout, time_elapsed;
	timed_chunk_t timed_chunk;
	
	int i;
	for(i=0;i<2*window->size;i++){
		timed_chunk = window->timed_chunks[i]
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
memchunk_t window_get_next(window_t window){
	timed_chunk_t timed_chunk = (timed_chunk_t)queue_pop(window->to_send);
	if(timed_chunk){
		timed_chunk_start(timed_chunk);	
		return timed_chunk->chunk;
	}
	else{
		return NULL;
	}
}
		


