#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>

#include "recv_window.h"
#include "ext_array.h"
#include "queue.h"

#define QUEUE_CAPACITY 1024

recv_chunk_t recv_chunk_init(uint32_t seq, void* data, int l){
	recv_chunk_t rc = malloc(sizeof(struct recv_chunk));
	rc->seqnum = seq;
	rc->data = data;
	rc->length = l;
	return rc;
}

// this destroys the data it was holding!!
void recv_chunk_destroy(recv_chunk_t* rc){
	free((*rc)->data);
	free(*rc);
	*rc=NULL;
}
	
int recv_chunk_compare(recv_chunk_t a, recv_chunk_t b){
	if(a->seqnum < b->seqnum) return -1;
	else return 1;
}

/*
			RECV WINDOW 
*/

struct recv_window {
	ext_array_t data_queue;
	sorted_list_t chunks_received;

	uint16_t size;
	uint16_t available_size;
	uint32_t left;
	uint32_t read_left;
//	recv_window_chunk_t* slider;
	pthread_mutex_t mutex;
};

recv_window_t recv_window_init(uint16_t window_size, uint32_t ISN){
	recv_window_t recv_window = (struct recv_window*)malloc(sizeof(struct recv_window));
	recv_window->data_queue = ext_array_init(QUEUE_CAPACITY);
	recv_window->chunks_received = sorted_list_init((comparator_f)recv_chunk_compare);
	recv_window->size = window_size;
	recv_window->available_size = window_size;
	
	/* the next byte you're expecting is the one after the 
		first sequence number */
	recv_window->read_left = recv_window->left = (ISN + 1) % MAX_SEQNUM;

	/* initialize your mutex */
	pthread_mutex_init(&(recv_window->mutex), NULL);

	return recv_window;
}

/*
_validate_seqnum
	takes a window and a sequence number along with its length and 
	determines whether or not the sequence number is valid. 
	
returns
	>0 if successful (offset at which the first byte from data should be read)
	-1 otherwise
*/
static int _validate_seqnum(recv_window_t recv_window, uint32_t seqnum, uint32_t length){
	uint32_t window_min = recv_window->left,
			 window_max = (recv_window->left+recv_window->size) % MAX_SEQNUM;
		
	/*  A segment is judged to occupy a portion of valid receive sequence
  		space if

		length 		window size		test for validity
		0			0				seqnum == RCV.NEXT
		0			>0				RCV.NEXT <= seqnum < RCV.NEXT+RCV.WND
		>0			0 				not acceptable
		>0 			>0				RCV.NEXT <= seqnum < RCV.NEXT+RCV.WND or
									RCV.NEXT <= seqnum + SEQ.LEN-1 < RCV.NEXT+RCV.WND
   	*/
	
	if( length == 0 ){
		if( recv_window->available_size == 0){
			if( seqnum==recv_window->left ) 
				return 0;
			else 
				return -1;
		}
	
		else{ /* (recv_window->size > 0) */
			if(BETWEEN_WRAP(seqnum, window_min, window_max)) 
				return 0;
			else 
				return -1; 
		}
	}
	else{ /* length > 0 */
		if(recv_window->available_size == 0)
			return -1;

		else if (BETWEEN_WRAP(seqnum, window_min, window_max)) 
			return 0;
	
		else if (BETWEEN_WRAP((seqnum+length)%MAX_SEQNUM, window_min, window_max))
			return WRAP_DIFF(seqnum, window_min, MAX_SEQNUM);

		else 
			return -1;
	}
}

/* we've received data up until the left of the window */
uint32_t recv_window_get_ack_synchronized(recv_window_t recv_window){
	return recv_window->read_left;
}

uint32_t recv_window_get_ack(recv_window_t recv_window){
	pthread_mutex_lock(&(recv_window->mutex));
	uint32_t ret = recv_window_get_ack_synchronized(recv_window);
	pthread_mutex_unlock(&(recv_window->mutex));
	return ret;
}

/* returns the current size of the window, which is currently dynamic */
uint16_t recv_window_get_size(recv_window_t recv_window){
	return recv_window->available_size;
}

/* 
recv_window_receive
	takes in a window, a pointer, the length associated with the memory pointed to by
	that pointer, and also the sequence number of the given data (the first octet of that
	data). This function performs all the necessary sliding window functions, and then returns
	the ACK number to send back. If there is no such number (ie the window didn't slide at all, 
	it will return -1.

	stores the data directly, so give it something that it can free!
*/
void recv_window_receive_synchronized(recv_window_t recv_window, void* data, uint32_t length, uint32_t seqnum){
	int offset = _validate_seqnum(recv_window, seqnum, length);
	if(offset<0){
		LOG(("seqnum %d not accepted. left: %d\n", seqnum, recv_window->left)); 
		return;
	}
	
	uint32_t to_write = MIN(length-offset, WRAP_DIFF(seqnum, (recv_window->left+recv_window->size)%MAX_SEQNUM, MAX_SEQNUM));

	/* if the data has a non-zero offset, then we will need to make a new chunk
		   of data that the recv_chunk can point to so that when it frees that void* 
		   pointer it actually does something. Let me illustrate:

		   void* chunk = malloc(100);
		   free(chunk+1); // LEAK!
		   free(chunk);   // good
	*/
 	if(offset > 0){
 		void* new_data = malloc(to_write);
 		memcpy(new_data, data+offset, to_write);
 		free(data);
 		data = new_data;
 	}

	int already_read_overlap = WRAP_DIFF((seqnum+offset)%MAX_SEQNUM, recv_window->read_left, MAX_SEQNUM);
	if(already_read_overlap >= 0)
	{
		to_write -= already_read_overlap;

		ext_array_push(recv_window->data_queue, data+already_read_overlap, to_write);
		free(data);

		recv_window->read_left      += to_write;
		recv_window->available_size -= to_write;

		/* then go through and see if this connects to the next received_chunk */
		recv_chunk_t next_chunk;
		
		int overlap;
		while((next_chunk = sorted_list_peek(recv_window->chunks_received))){		

			/* if the next chunk is farther along then the last byte that's been read, 
			   then just continue */
			if(WRAP_DIFF(next_chunk->seqnum, recv_window->read_left, MAX_SEQNUM) < 0) 
				break;
			
			// better be the same!
			next_chunk = sorted_list_pop(recv_window->chunks_received);

			/* check the overlap between the chunk that you've previously
			   received and the chunk that was just read */
			overlap = WRAP_DIFF(next_chunk->seqnum, recv_window->read_left, MAX_SEQNUM);

			/* if the overlap is less than the length of the chunk (ie the 
			   chunk that was in the received list isn't completely covered
			   by the chunk taht was just pushed) then push its data */
			if(overlap < next_chunk->length){
				ext_array_push(recv_window->data_queue, next_chunk->data+overlap, next_chunk->length-overlap);
	
				/* only incremente the read_left/decrement the available size by 
				   the amount that you actually used from the most recent chunk */
				recv_window->read_left 		+= next_chunk->length-overlap;
				recv_window->available_size -= next_chunk->length-overlap;
			}
			
			/* destroy that chunk */
			free(next_chunk->data);
			recv_chunk_destroy(&next_chunk);
		}
	}
	else
	{
 		sorted_list_insert(recv_window->chunks_received, recv_chunk_init(seqnum, data, to_write)); 
	}	
}
  
void recv_window_receive(recv_window_t recv_window, void* data, uint32_t length, uint32_t seqnum){
	pthread_mutex_lock(&(recv_window->mutex));
	recv_window_receive_synchronized(recv_window, data, length, seqnum);
	pthread_mutex_unlock(&(recv_window->mutex));
}

/*
recv_window_get_next
	gets the next recv_window_chunk_t from the to_read queue. 

	returns
		recv_window_chunk_t pointing to the next thing that the application
		should read. Remember when reading from this that there is a field
		called 'offset' which indicates from what offset to start reading
		the payload of the window_chunk
		
		NULL if there is nothing
*/
memchunk_t recv_window_get_next_synchronized(recv_window_t recv_window, int bytes){
	memchunk_t got = ext_array_peel(recv_window->data_queue, bytes); 
	if(!got)
		return NULL;
	
	recv_window->available_size+=got->length;
	return got;
}

memchunk_t recv_window_get_next(recv_window_t recv_window, int bytes){
	pthread_mutex_lock(&(recv_window->mutex));
	memchunk_t chunk = recv_window_get_next_synchronized(recv_window, bytes);
	pthread_mutex_unlock(&(recv_window->mutex));
	return chunk;
}

/*
recv_window_destroy
	destroys the window AND ALL OF THE DATA IN IT. ie it frees the data
	that was being stored in the window_chunks in both the buffer for incoming
	data and the to_read queue (the intersection of these things had better
	be null
*/
void recv_window_destroy(recv_window_t* recv_window){
	ext_array_destroy(&((*recv_window)->data_queue));
	sorted_list_destroy_total(&((*recv_window)->chunks_received), (destructor_f)recv_chunk_destroy);

	pthread_mutex_destroy(&((*recv_window)->mutex));
	free(*(recv_window));
	*recv_window = NULL;
}
