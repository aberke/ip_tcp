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

/////////////// AUXILIARY DATA STRUCTURES /////////////////////////

///////////// WINDOW CHUNK ////////////

/* gets data from left all the way up to (but NOT including) right */
send_window_chunk_t send_window_chunk_init(send_window_t send_window, void* data, int length, uint32_t seqnum){
	send_window_chunk_t send_window_chunk = malloc(sizeof(struct send_window_chunk));
	
	gettimeofday(&(send_window_chunk->send_time), NULL);
	send_window_chunk->data   = data;
	send_window_chunk->seqnum = seqnum;
	send_window_chunk->length = length;
	send_window_chunk->resent = 0;
	
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
/* RFC:     An Example Retransmission Timeout Procedure

      Measure the elapsed time between sending a data octet with a
      particular sequence number and receiving an acknowledgment that
      covers that sequence number (segments sent do not have to match
      segments received).  This measured elapsed time is the Round Trip
      Time (RTT).  Next compute a Smoothed Round Trip Time (SRTT) as:

        SRTT = ( ALPHA * SRTT ) + ((1-ALPHA) * RTT)

      and based on this, compute the retransmission timeout (RTO) as:

        RTO = min[UBOUND,max[LBOUND,(BETA*SRTT)]]

      where UBOUND is an upper bound on the timeout (e.g., 1 minute),
      LBOUND is a lower bound on the timeout (e.g., 1 second), ALPHA is
      a smoothing factor (e.g., .8 to .9), and BETA is a delay variance
      factor (e.g., 1.3 to 2.0). */
///////////// WINDOW //////////////////
struct send_window{
	ext_array_t data_queue;
	plain_list_t sent_list;
	queue_t timed_out_chunks;

	uint32_t sent_left;	
	uint32_t size;
	uint32_t left;
	uint32_t send_size;

	/* for being thread safe,
		synchronizes over all functions */
	pthread_mutex_t mutex;
	
	/* for calculating RTO */	
	double RTO;
	double SRTT;
	double ALPHA;
	double BETA;
	double UBOUND; //upper bound
	double LBOUND; //lower bound
};
// recalculates SRTT and RTO and returns new RTO
double _recalculate_RTO(send_window_t send_window, double RTT){
	/* RFC:       SRTT = ( ALPHA * SRTT ) + ((1-ALPHA) * RTT)
					 and based on this, compute the retransmission timeout (RTO) as:
				  RTO = min[UBOUND,max[LBOUND,(BETA*SRTT)]] 
	*/
	if((send_window->SRTT == 0) && (RTT != 0)){
		//let's set our first SRTT value as this first RTT value
		send_window->SRTT = RTT;
	}
	else{
		send_window->SRTT = ((send_window->ALPHA)*(send_window->SRTT)) + ((1-send_window->ALPHA)*RTT);
	}
	send_window->RTO = 1.0;
	if(send_window->SRTT == 0){
		return 1.0;
	}
	//send_window->RTO = MIN(send_window->UBOUND, MAX(send_window->LBOUND, (send_window->BETA)*(send_window->SRTT)));
	
	//print(("new RTT: %f, new SRTT: %f, new RTO: %f", RTT, send_window->SRTT, send_window->RTO), SEND_WINDOW_PRINT);
	
	return send_window->RTO;
}

send_window_t send_window_init(int window_size, int send_size, int ISN, 
								double ALPHA, double BETA, double UBOUND, double LBOUND){
	send_window_t send_window = (send_window_t)malloc(sizeof(struct send_window));

	send_window->data_queue 	  = ext_array_init(QUEUE_CAPACITY);
	send_window->sent_list		  = plain_list_init();
	send_window->timed_out_chunks = queue_init();

	send_window->send_size  = send_size;
	send_window->size = window_size;
	send_window->left = send_window->sent_left = ISN;

	pthread_mutex_init(&(send_window->mutex), NULL);
	
	send_window->ALPHA = ALPHA;
	send_window->BETA = BETA;
	send_window->UBOUND = UBOUND;
	send_window->LBOUND = LBOUND;
	send_window->RTO = 0;
	send_window->SRTT = 0;
	_recalculate_RTO(send_window, 0);
	
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

double send_window_get_RTO(send_window_t send_window){
	return send_window->RTO;
}

void send_window_set_size(send_window_t send_window, uint32_t size){
	pthread_mutex_lock(&(send_window->mutex));
	send_window->size = size;
	pthread_mutex_unlock(&(send_window->mutex));
}

int send_window_validate_ack(send_window_t send_window, uint32_t ack){
    if(ack==(send_window->left) || ack==(send_window->left+1))
        return 0;
    
	if( BETWEEN_WRAP(ack, send_window->left, (send_window->left+send_window->size)%MAX_SEQNUM) 
		|| ack==((send_window->left+send_window->size+1)%MAX_SEQNUM)){
		return 0;
	}
	return -1;
}

void send_window_push_synchronized(send_window_t send_window, void* data, int length){
	ext_array_push(send_window->data_queue, data, length);
}

void send_window_set_seq(send_window_t send_window, uint32_t seq){
     send_window->left = send_window->sent_left = seq;
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
		/* restart its timer (it's still on the sent list!) */
		gettimeofday(&(sw_chunk->send_time), NULL);
		return sw_chunk;
	}

	uint32_t sent_left = send_window->sent_left;

	int left_in_window = WRAP_DIFF(sent_left, (send_window->left+send_window->size)%MAX_SEQNUM, MAX_SEQNUM);
	int to_send = MIN(send_window->send_size, left_in_window);

	if(to_send <= 0) 
	 	return NULL;

	memchunk_t chunk = ext_array_peel(send_window->data_queue, to_send);
	if(!chunk)	
		return NULL;

	/* generate the chunk to send and add it to the sent_list */
	sw_chunk = send_window_chunk_init(send_window, chunk->data, chunk->length, sent_left);
	free(chunk);
	plain_list_append(send_window->sent_list, sw_chunk);

	/* increment the sent_left */
	send_window->sent_left = (sent_left + chunk->length) % MAX_SEQNUM;

	//printf("[sw chunk size: %d] [regular chunk size: %d]\n", sw_chunk->length, chunk->length);
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
		print(("Received invalid seqnum: %d, current send_window_min: %d, send_window_max: %d\n", seqnum, send_window_min, send_window_max), SEND_WINDOW_PRINT); 
		return; 
	}

	send_window->left = seqnum;
	
	plain_list_t list = send_window->sent_list;
	plain_list_el_t el;
	send_window_chunk_t chunk;
	
	double RTT;
	struct timeval now, chunk_timer;
	gettimeofday(&now, NULL);

	PLAIN_LIST_ITER(list, el)
		chunk = (send_window_chunk_t)el->data;
		if(BETWEEN_WRAP(seqnum, chunk->seqnum, (chunk->seqnum+chunk->length)%MAX_SEQNUM)){
			/* this is the chunk containing the ack, so move the pointer of 
				chunk up until its pointing to the as-of-yet unsent data */ 
			chunk->offset += WRAP_DIFF(chunk->seqnum, seqnum, MAX_SEQNUM);
			if(!chunk->resent){
				chunk_timer = chunk->send_time;
				RTT = now.tv_sec - chunk_timer.tv_sec;
				RTT += now.tv_usec/1000000.0 - chunk_timer.tv_usec/1000000.0;
				_recalculate_RTO(send_window, RTT);						
			}
			break;
		}

		/* you can free this chunk because it's been acked (all data has 
			been received UP TO the given seqnum */
		free(chunk->data);
		free(chunk);
	
		/* you can delete this link in the list */
		print(("removing from sentlist: %p\n", chunk), SEND_WINDOW_PRINT);
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
   queue and resets the timer. 
   
   ALEX gave this a return value -- I think it returns number of outstanding segments, let me know if wrong
   */
int send_window_check_timers_synchronized(send_window_t send_window){

	/* get the time */
	struct timeval now, chunk_timer;
	gettimeofday(&now, NULL);
	float time_elapsed;
	
	plain_list_t list = send_window->sent_list;
	plain_list_el_t el;
	send_window_chunk_t chunk;
	
	int i = 0;
	PLAIN_LIST_ITER(list, el)
		i++;
		chunk = (send_window_chunk_t)el->data;
		chunk_timer = chunk->send_time;
		
		time_elapsed = now.tv_sec - chunk_timer.tv_sec;
		time_elapsed += now.tv_usec/1000000.0 - chunk_timer.tv_usec/1000000.0;
		
		if(time_elapsed > send_window->RTO){
			print(("------------resending---------------"), SEND_WINDOW_PRINT);
			chunk->resent = (chunk->resent) + 1;
			queue_push(send_window->timed_out_chunks, (void*)chunk);
		}
		
	PLAIN_LIST_ITER_DONE(list);

	return i;
}

/* Alex wants to be able to use this for closing purposes as well
	-- so if there are no more timers to check -- all data sent successfully acked 
	-- then we cant continue with close
	returns 0 if no more outstanding segmements -- all data sent acked
	returns > 0 number for remaining outstanding segments */
int send_window_check_timers(send_window_t sw){
	pthread_mutex_lock(&(sw->mutex));
	int ret = send_window_check_timers_synchronized(sw);
	pthread_mutex_unlock(&(sw->mutex));
	return ret;
}

// needed for driver window_cmd
int send_window_get_size(send_window_t send_window){
	return send_window->size;
}

void send_window_print(send_window_t send_window){
	print(("Left: %d\nsize: %d\nSent_left: %d\n", send_window->left, send_window->size, send_window->sent_left), SEND_WINDOW_PRINT);
}


