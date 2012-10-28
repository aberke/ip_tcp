#include <stdlib.h>
#include <string.h>

#include "recv_window.h"
#include "queue.h"

/*
recv_window_chunk_init
	constructs and passes back a recv_window_chunk which wraps around the
	passed in data, and sets its length, while also setting offset to 0
*/
recv_window_chunk_t recv_window_chunk_init(void* data, int length){
	recv_window_chunk_t rwc = malloc(sizeof(struct recv_window_chunk));
	rwc->data = malloc(length);
	memcpy(rwc->data,data,length);
	rwc->offset = 0;
	rwc->length = length;
	return rwc;
}

/* 
recv_window_chunk_destroy
	run-of-the-mill destructor. Free the associated data, sets the given 
	pointer to NULL, and does NOT touch the chunk of memory that its
	data field points to 
*/
void recv_window_chunk_destroy(recv_window_chunk_t* rwc){
	free(*rwc);
	*rwc = NULL;
}

/*
recv_window_chunk_destroy
	takes a destructor and uses it to "destroy" its payload (the memory
	pointed to by its 'data' field. Then calls destroy on the recv_window_chunk
	itself
*/
void recv_window_chunk_destroy_total(recv_window_chunk_t* rwc, destructor_f destructor){
	destructor(&((*rwc)->data));
	recv_window_chunk_destroy(rwc);
}

/*
recv_window_chunk_destroy_free
	free's the data that it's payload points to. This is simply a facilitating
	method for handing off to queue_destroy (because what we really want to do
	is queue_destroy(queue, lambda x: recv_window_chunk_destroy(x, util_free)) but
	of course we can't do that now can we
*/
void recv_window_chunk_destroy_free(recv_window_chunk_t* rwc){
	free((*rwc)->data);
	recv_window_chunk_destroy(rwc);
}

/*
recv_window_chunk_decrement
	takes a pointer to a pointer to a recv_window_chunk, this is due to 
	its job as a potential "garbage" collector. It increments the offset of 
	the chunk, and then correspondingly decreases the length. If the length
	is 0, this chunk is no longer needed so it is "cleaned up"
*/ 
void recv_window_chunk_decrement(recv_window_chunk_t* rwc){
	(*rwc)->offset++;
	(*rwc)->length--;
	if((*rwc)->length==0)
		recv_window_chunk_destroy_total(rwc, util_free);
}
	
/*
			RECV WINDOW 
*/

struct recv_window {
	queue_t to_read;
	int size;
	int left;
	recv_window_chunk_t* slider;
};

recv_window_t recv_window_init(int window_size, int ISN){
	recv_window_t recv_window = (struct recv_window*)malloc(sizeof(struct recv_window));
	recv_window->size = window_size;
	recv_window->left = ISN;
	recv_window->slider = malloc(sizeof(recv_window_chunk_t)*(window_size+1));	
	memset(recv_window->slider, 0, sizeof(recv_window_chunk_t)*(window_size+1));
	recv_window->to_read = queue_init();
	return recv_window;
}

/*
_validate_seqnum
	takes a window and a sequence number along with its length and 
	determines whether or not the sequence number is valid. 
	
returns
	0  successful (valid seqnum)
	-1 otherwise
*/
static int _validate_seqnum(recv_window_t recv_window, int seqnum, int length){
	int window_min = recv_window->left,
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
		if( recv_window->size == 0){
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
		if(recv_window->size == 0)
			return -1;

		else{ /* recv_window->size > 0 */
			if(BETWEEN_WRAP(seqnum, window_min, window_max) || BETWEEN_WRAP((seqnum+length)%MAX_SEQNUM, window_min, window_max))
				return 0;
			else 
				return -1;
		}
	}
}


/* 
recv_window_receive
	takes in a window, a pointer, the length associated with the memory pointed to by
	that pointer, and also the sequence number of the given data (the first octet of that
	data). This function performs all the necessary sliding window functions, and then returns
	the ACK number to send back. If there is no such number (ie the window didn't slide at all, 
	it will return -1.
*/
int recv_window_receive(recv_window_t recv_window, void* data, int length, int seqnum){
	if(_validate_seqnum(recv_window, seqnum, length) < 0){
		LOG(("seqnum %d not accepted. left: %d\n", seqnum, recv_window->left)); 
		return -1;
	}


	/* this will be what we store in the receiving buffer */
	recv_window_chunk_t to_store = recv_window_chunk_init(data, length);

	int i, index;
	recv_window_chunk_t stored;
	for(i=0;i<length;i++){

		index = (seqnum + i) % (recv_window->size+1);
		if (index==window_max) 
			break;

		stored = recv_window->slider[index];
		if(stored){
			recv_window_chunk_decrement(&stored);
		}

		recv_window->slider[index] = to_store;
	}

	/* if you didn't actually get placed into the array, then why
		do we need you at all? */
	if(i==0)
		recv_window_chunk_destroy_total(&to_store,util_free);
	
	/* otherwise reset the length of the chunk that we're storing
		to match what was actually stored (maybe we are trying to 
		receive more than was available in the window, so this 
		might not be equal to the original length */
	else	
		to_store->length = i;

	/* now check to see if this is the left-most piece of the window. 
		If it is, then we can pass it along with all of the following
		chunks that already in the window up to the application (ie
		push them on the to_read queue) */
	if(seqnum==recv_window->left){	

		queue_push(recv_window->to_read, to_store);
		recv_window_chunk_t just_pushed = to_store;

		int j;
		for(j=0;j<recv_window->size;j++){
			index = (seqnum+j) % (recv_window->size+1);
			if((seqnum+j % MAX_SEQNUM)==window_max)
				break;

			stored = recv_window->slider[index];			
	
			/* a NULL pointer in the slider array indicates that we haven't
				received that data yet, so we're done iterating through */
			if(!stored)
				break; 

			/* if you've found something in the array that isn't the thing
				you just pushed, then push it. ASSUMPTION: all the pointers
				that point to a particular recv_window_chunk are contiguous
				so recv_window->slider[index] = NULL will set all of them
				to NULL so the only access to that chunk will be from the 
				queue. If this does not hold, then bad things could happen */
			if(stored != just_pushed){
				queue_push(recv_window->to_read, to_store);
				just_pushed = stored;
			}

			recv_window->slider[index] = NULL;
		}
		recv_window->left = (recv_window->left + j) % MAX_SEQNUM;
		return recv_window->left;
	}
	return -1;
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
recv_window_chunk_t recv_window_get_next(recv_window_t recv_window){
	return (recv_window_chunk_t)queue_pop(recv_window->to_read);
}

/*
recv_window_destroy
	destroys the window AND ALL OF THE DATA IN IT. ie it frees the data
	that was being stored in the window_chunks in both the buffer for incoming
	data and the to_read queue (the intersection of these things had better
	be null
*/
void recv_window_destroy(recv_window_t* recv_window){
	queue_destroy_total(&((*recv_window)->to_read),(destructor_f)recv_window_chunk_destroy_free);

	int i; 
	for(i=0;i<(*recv_window)->size+1;i++){
		if((*recv_window)->slider[i])
			recv_window_chunk_destroy_total(&((*recv_window)->slider[i]), util_free);
	}
	
	free((*recv_window)->slider);

	free(*(recv_window));
	*recv_window = NULL;
}


