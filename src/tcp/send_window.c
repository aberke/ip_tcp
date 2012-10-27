#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "queue.h"
#include "ext_array.h"
#include "send_window.h"
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

#define send_window_memcpy(win, src, length) 						\
do{																\
	int write_right = MIN(((win)->size+1) - (((win)->right)%((win)->size+1)), length);\
	memcpy(((win)->slider)+(((win)->right)%((win)->size+1)), 	\
			(src), write_right);								\
	memcpy(((win)->slider)+0, (src)+write_right, (length)-write_right);\
} 																\
while(0)

#define send_window_get_seqnum(send_window, offset)\
	((send_window->wrap_count)*((send_window->size)+1)+(offset)) % MAX_SEQNUM 

typedef struct timed_chunk* timed_chunk_t;

///////////// WINDOW //////////////////
struct send_window{
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
window_chunk_t window_chunk_init(send_window_t send_window, int left, int right){
	window_chunk_t window_chunk = malloc(sizeof(struct window_chunk));
	int wrap_length = WRAP_DIFF(left, right, MAX_SEQNUM);
	window_chunk->data = malloc(wrap_length);
	wrap_src_memcpy(window_chunk->data, send_window->slider, (left%(send_window->size+1)), wrap_length, send_window->size+1);

	window_chunk->seqnum = send_window_get_seqnum(send_window, left);
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

void _free_timers(send_window_t* send_window);

/////////////////////////

send_window_t send_window_init(double timeout, int send_window_size, int send_size, int ISN){
	send_window_t send_window = (send_window_t)malloc(sizeof(struct send_window));

	send_window->data_queue = ext_array_init(QUEUE_CAPACITY);
	send_window->to_send    = queue_init();

	send_window->timeout 	= timeout;
	send_window->send_size   = send_size;
	send_window->size 		= send_window_size;
	send_window->slider  	= malloc(send_window_size+1);
	memset(send_window->slider, 0, send_window_size);

	send_window->timed_chunks = malloc(sizeof(timed_chunk_t)*(send_window_size+1));
	memset(send_window->timed_chunks, 0, sizeof(timed_chunk_t)*(send_window_size+1));
	
	/* this will be a placeholder for when we have acked a time_chunk, but it
		isn't on the left of the send_window */
	send_window->acked_chunk_placeholder = timed_chunk_init(0, 0);

	send_window->left = send_window->right = send_window->sent_left = ISN;
	send_window->wrap_count = 0;
	send_window->ISN = ISN;

	return send_window;
}

void send_window_destroy(send_window_t* send_window){
	ext_array_destroy(&((*send_window)->data_queue));

	_free_timers(send_window);

	queue_destroy_total(&((*send_window)->to_send), (destructor_f)window_chunk_destroy_free);
	free((*send_window)->acked_chunk_placeholder);
	free((*send_window)->slider);
	free((*send_window)->timed_chunks);
	free(*send_window);
	*send_window = NULL;
}

void send_window_push(send_window_t send_window, void* data, int length){

	int left_in_send_window = send_window->size - WRAP_DIFF(send_window->left, send_window->right, MAX_SEQNUM);
	int to_write       = MIN(left_in_send_window, length);
	send_window_memcpy(send_window, data, to_write);
	ext_array_push(send_window->data_queue, data+to_write, length-to_write);
	send_window->right = (send_window->right + to_write) % MAX_SEQNUM;
	
}

window_chunk_t send_window_get_next(send_window_t send_window){
	timed_chunk_t chunk;
	if((chunk = (timed_chunk_t)queue_pop(send_window->to_send)) == NULL){
		int left_to_send = WRAP_DIFF(send_window->sent_left, send_window->right, MAX_SEQNUM);
		if(!left_to_send) 
			return NULL;
		else 
		{
			int chunk_size = MIN(send_window->send_size, left_to_send);
			chunk 	  	   = timed_chunk_init(send_window->sent_left, (send_window->sent_left + chunk_size) % MAX_SEQNUM);

			int i;
			for(i=0;i<chunk_size;i++){
				send_window->timed_chunks[(send_window->sent_left + i) % (send_window->size + 1)] = chunk;
			}

			send_window->sent_left = (send_window->sent_left + chunk_size) % MAX_SEQNUM;
		}
	}

	time(&(chunk->start_time));
	chunk->status = WAITING;

	return window_chunk_init(send_window, chunk->left, chunk->right);
}

void send_window_ack(send_window_t send_window, int seqnum){

	int send_window_min = send_window->left,
		send_window_max = send_window->right;
	
	
	if(send_window_min < send_window_max && !BETWEEN(seqnum, send_window_min, send_window_max)
		|| send_window_min > send_window_max && !BETWEEN(seqnum, send_window_min, send_window_min + send_window_max))
		{ LOG(("Received invalid seqnum: %d, current send_window_min: %d, send_window_max: %d\n", seqnum, send_window_min, send_window_max)); return; }

	seqnum = seqnum==0 ? MAX_SEQNUM  : seqnum-1;
	
	timed_chunk_t acked_chunk = send_window->timed_chunks[(seqnum)%(send_window->size+1)];
	if(!acked_chunk)
		{ LOG(("NULL acked_chunk")); return; }


	int acked_left = acked_chunk->left;
	if (send_window->left == acked_left) {
		int acked_right = seqnum;
		do{ 
			acked_right = (acked_right+1) % MAX_SEQNUM;
			if(CONGRUENT(acked_left, acked_right, send_window->size+1)) break;
		}
		while(send_window->timed_chunks[(acked_right % (send_window->size+1))] == send_window->acked_chunk_placeholder);

		int i;
		for( i=(acked_left%(send_window->size+1));
				!CONGRUENT(i, acked_right, send_window->size+1);
					i=(i+1)%(send_window->size+1)) {
			send_window->timed_chunks[i] = NULL;
		}
	
		free(acked_chunk);

		send_window->left = acked_right;
		memchunk_t from_queue = ext_array_peel(send_window->data_queue, send_window->size-WRAP_DIFF(send_window->left, send_window->right, MAX_SEQNUM));
		if(from_queue){
			send_window_memcpy(send_window, from_queue->data, from_queue->length);
			send_window->right = (send_window->right + from_queue->length) % MAX_SEQNUM;
			memchunk_destroy_total(&from_queue, util_free);
		}
	}
	else{
		int i;
		for(i=(acked_left%(send_window->size+1));
			!CONGRUENT(i,seqnum,(send_window->size+1));
				i=(i+1)%(send_window->size+1)) 
			send_window->timed_chunks[i] = send_window->acked_chunk_placeholder;
		acked_chunk->left=seqnum;
	}
}

/* Will handle going through all the timers and for the ones who have 
   timers that have elapsed time > timeout, adds these to the to_send
   queue and resets the timer. */
void send_window_check_timers(send_window_t send_window){
	timed_chunk_t timed_chunk = NULL;

	/* get the time */
	time_t now;
	time(&now);

	int i;
	for(i=0;i<(send_window->size+1);){

		/* find the next timed_chunk that isn't the current one */
		while( i<(send_window->size+1) && send_window->timed_chunks[i] == timed_chunk) i++;
		timed_chunk = send_window->timed_chunks[i];

		/* if the new chunk is actually a chunk, then check if it's waiting for an ack,
			and if it is then check the time-elapsed. If more than send_window->timeout, then
			push it onto the front of the to_send queue, and set its status to NOT_WAITING
			(because it hasn't been sent yet) */
		if(timed_chunk && 
				timed_chunk->status == WAITING && 
					difftime(now, timed_chunk->start_time) > send_window->timeout) {
	
			queue_push_front(send_window->to_send, (void*)timed_chunk);
			timed_chunk->status = NOT_WAITING;
		}
	}
}	

void _free_timers(send_window_t* send_window){
	
	timed_chunk_t chunk;
	if((*send_window)->size > 0){
		chunk = (*send_window)->timed_chunks[0];

		int until;
		if (chunk)
			until = chunk->left == 0 ? (*send_window)->size : chunk->left%((*send_window)->size+1);
		else 
			until = (*send_window)->size;

		int i;
		for(i=0;i<until;){
			chunk = (*send_window)->timed_chunks[i];
			if(chunk && chunk != (*send_window)->acked_chunk_placeholder){
				i = chunk->right;
				free(chunk);
				if(i==0) break;
			}
			else i++;
		}	
	}
}
	
void send_window_print(send_window_t send_window){
	printf("Left: %d\nRight: %d\nSent_left: %d\n", send_window->left, send_window->right, send_window->sent_left);
}


