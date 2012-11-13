#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "queue.h"
#include "ext_array.h"
#include "send_window.h"
#include "utils.h"
#include "list.h"

#define QUEUE_CAPACITY 1024

#define wrap_src_memcpy(dst, src, src_offset, size, modulo)	\
do{															\
	int write_right = MIN((modulo) - (src_offset), size);	\
	memcpy(dst, (src)+(src_offset), write_right);			\
	memcpy((dst)+write_right, (src)+0, (size)-write_right);	\
} 															\
while(0)

#define send_window_memcpy(win, src, length) 					\
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
	plain_list_t sent_list;
	queue_t timed_out_chunks;

	uint32_t sent_left;	
	
	double timeout;
	uint32_t size;
	uint32_t left;
	uint32_t send_size;

	/* for being thread safe,
		synchronizes over all functions */
	pthread_mutex_t mutex;
};

/////////////// AUXILIARY DATA STRUCTURES /////////////////////////

///////////// WINDOW CHUNK ////////////

/* gets data from left all the way up to (but NOT including) right */
send_window_chunk_t send_window_chunk_init(send_window_t send_window, void* data, int length, uint32_t seqnum){
	send_window_chunk_t send_window_chunk = malloc(sizeof(struct send_window_chunk));

	send_window_chunk->data   = data;
	send_window_chunk->seqnum = seqnum;
	send_window_chunk->length = length;
	
	return send_window_chunk;
}

void send_window_chunk_destroy(send_window_chunk_t* wc){ 
	free(*wc); 
	*wc = NULL;
}

void send_window_chunk_destroy_total(send_window_chunk_t* wc, destructor_f destructor){
	destructor(&((*wc)->data));
	send_window_chunk_destroy(wc);
}	

void send_window_chunk_destroy_free(send_window_chunk_t* wc){
	free((*wc)->data);
	send_window_chunk_destroy(wc);
}


/////////////////////////

send_window_t send_window_init(double timeout, int window_size, int send_size, int ISN){
	send_window_t send_window = (send_window_t)malloc(sizeof(struct send_window));

	send_window->data_queue 	  = ext_array_init(QUEUE_CAPACITY);
	send_window->sent_list		  = plain_list_init();
	send_window->timed_out_chunks = queue_init();

	send_window->timeout 	= timeout;
	send_window->send_size  = send_size;
	send_window->size = window_size;
	send_window->left = send_window->sent_left = ISN;

	pthread_mutex_init(&(send_window->mutex), NULL);
	
	return send_window;
}

void send_window_destroy(send_window_t* send_window){
	ext_array_destroy(&((*send_window)->data_queue));

	plain_list_destroy_total(&((*send_window)->sent_list), (destructor_f)send_window_chunk_destroy_free);
	queue_destroy(&((*send_window)->timed_out_chunks));
	pthread_mutex_destroy(&((*send_window)->mutex));

	free(*send_window);
	*send_window = NULL;
}

void send_window_set_window_size(send_window_t send_window, uint32_t size){
	pthread_mutex_lock(&(send_window->mutex));
	send_window->size = size;
	pthread_mutex_unlock(&(send_window->mutex));
}

int send_window_validate_ack(send_window_t send_window, uint32_t ack){
	if( BETWEEN_WRAP(ack, send_window->left, (send_window->left+send_window->size)%MAX_SEQNUM) 
		|| ack==((send_window->left+send_window->size+1)%MAX_SEQNUM)){
		return 0;
	}
	return -1;
}

void send_window_push_synchronized(send_window_t send_window, void* data, int length){
	ext_array_push(send_window->data_queue, data, length);
}

void send_window_push(send_window_t sw, void* d, int l){
	pthread_mutex_lock(&(sw->mutex));
	send_window_push_synchronized(sw, d, l);
	pthread_mutex_unlock(&(sw->mutex));
}

// just add a function for getting the next sequence number
// that you're going to send 
uint32_t send_window_get_next_seq_synchronized(send_window_t send_window){
	return send_window->sent_left;
}

uint32_t send_window_get_next_seq(send_window_t send_window){
	pthread_mutex_lock(&(send_window->mutex));
	uint32_t ret = send_window_get_next_seq_synchronized(send_window);
	pthread_mutex_unlock(&(send_window->mutex));
	return ret;
}

send_window_chunk_t send_window_get_next_synchronized(send_window_t send_window){
	send_window_chunk_t sw_chunk;
	if((sw_chunk=(send_window_chunk_t)queue_pop(send_window->timed_out_chunks)) != NULL){
		return sw_chunk;
	}

	uint32_t sent_left = send_window->sent_left;
	memchunk_t chunk = ext_array_peel(send_window->data_queue, MIN(send_window->send_size, send_window->left+send_window->size - sent_left));
	if(!chunk)	
		return NULL;

	sw_chunk = malloc(sizeof(struct send_window_chunk));
	sw_chunk->data = chunk->data;
	sw_chunk->length = chunk->length;
	sw_chunk->seqnum = sent_left;

	send_window->sent_left = (sent_left + chunk->length) % MAX_SEQNUM;
	free(chunk);
	return sw_chunk;
}

send_window_chunk_t send_window_get_next(send_window_t send_window){
	pthread_mutex_lock(&(send_window->mutex));
	send_window_chunk_t chunk = send_window_get_next_synchronized(send_window);
	pthread_mutex_unlock(&(send_window->mutex));
	return chunk;
}

void send_window_ack_synchronized(send_window_t send_window, int seqnum){

	int send_window_min = send_window->left,
		send_window_max = (send_window->left+send_window->size) % MAX_SEQNUM;
	
	if(seqnum==send_window->left || seqnum==(send_window->left+send_window->size+1)%MAX_SEQNUM)
		return;

	if(!BETWEEN_WRAP(seqnum, send_window_min, send_window_max)){ 
		LOG(("Received invalid seqnum: %d, current send_window_min: %d, send_window_max: %d\n", seqnum, send_window_min, send_window_max)); 
		return; 
	}

	send_window->left = seqnum;
	
	plain_list_t list = send_window->sent_list;
	plain_list_el_t el;
	send_window_chunk_t chunk;

	PLAIN_LIST_ITER(list, el)
		chunk = (send_window_chunk_t)el->data;
		if(BETWEEN_WRAP(seqnum, chunk->seqnum, (chunk->seqnum+chunk->length)%MAX_SEQNUM)){
			/* this is the chunk containing the ack, so move the pointer of 
				chunk up until its pointing to the as-of-yet unsent data */ 
			chunk->offset += WRAP_DIFF(chunk->seqnum, seqnum, MAX_SEQNUM);
			break;
		}

		/* you can free this chunk because it's been acked (all data has 
			been received UP TO the given seqnum */
		free(chunk->data);
		free(chunk);
	
		/* you can delete this link in the list */
		plain_list_remove(list, el);
	PLAIN_LIST_ITER_DONE(list);

	return;
}	

void send_window_ack(send_window_t sw, int seqnum){
	pthread_mutex_lock(&(sw->mutex));
	send_window_ack_synchronized(sw, seqnum);
	pthread_mutex_unlock(&(sw->mutex));
}

/* Will handle going through all the timers and for the ones who have 
   timers that have elapsed time > timeout, adds these to the to_send
   queue and resets the timer. */
void send_window_check_timers_synchronized(send_window_t send_window){

	/* get the time */
	time_t now;
	time(&now);
	
	plain_list_t list = send_window->sent_list;
	plain_list_el_t el;
	send_window_chunk_t chunk;

	PLAIN_LIST_ITER(list, el)
		chunk = (send_window_chunk_t)el->data;
		if(difftime(now, chunk->send_time) > send_window->timeout){
			print(("resending"), SEND_WINDOW_PRINT);
			queue_push_front(send_window->timed_out_chunks, (void*)chunk);
		}
	PLAIN_LIST_ITER_DONE(list);

	return;
}

void send_window_check_timers(send_window_t sw){
	pthread_mutex_lock(&(sw->mutex));
	send_window_check_timers_synchronized(sw);
	pthread_mutex_unlock(&(sw->mutex));
}

// needed for driver window_cmd
int send_window_get_size(send_window_t send_window){
	return send_window->size;
}

void send_window_print(send_window_t send_window){
	printf("Left: %d\nsize: %d\nSent_left: %d\n", send_window->left, send_window->size, send_window->sent_left);
}


