#include <stdlib.h>
#include <string.h>

#include "recv_window.h"

struct recv_window {
	queue_t to_read;
	int window_size;
	int left, right;
	memchunk_t* slider;
};

recv_window_t recv_window_init(int window_size, int ISN){
	recv_window_t recv_window = (struct recv_window*)malloc(sizeof(struct recv_window));
	recv_window->size = window_size;
	recv_window->left = recv_window->right = ISN;
	recv_window->slider = malloc(sizeof(memchunk_t)*2*window_size);	
	memset(recv_window->slider, 0, sizeof(memchunk_t)*2*window_size);
	recv_window->to_read = queue_init();
	return recv_window;
}

void recv_window_receive(recv_window_t recv_window, window_chunk_t window_chunk){
	int seqnum = window_chunk->seqnum,
		window_min = recv_window->left,
		window_max = recv_window->right;
		
	/* make sure the seqnum is in the window of acceptable seqnums */
	if(window_min < window_max && !BETWEEN(seqnum, window_min, window_max) 
		|| window_min > window_max && !BETWEEN(seqnum, window_min, window_min+window_max))

		{ LOG(("seqnum %d not accepted. left: %d, right: %d\n", seqnum, recv_window->left, recv_window->right)); }

 	window_chunk_t already_recv = recv_window->slider[seqnum % recv_window->size*2];
	if(!already_recv)
		for(i=0;i<window_chunk->length;i++){
			if( recv_window->slider[(seqnum+i) % (recv_window->size*2)])
				break;
			recv_window->slider[(seqnum+i) % (recv_window->size*2)] = window_chunk;
		}
		window_chunk->

		recv_window->slider[seqnum % recv_window->size*2] = window_chunk;
	else
		
	

window_chunk_t recv_window_get_next(recv_window_t recv_window){
	return (window_chunk_t)queue_pop(recv_window->to_read);
}

void recv_window_destroy(recv_window_t* recv_window){

	int i; 
	for(i=0;i<2*(*recv_window)->size;i++){
		if((*recv_window)->slider[i])
			memchunk_destroy_total(&((*recv_window)->slider[i]), util_free);
	}
	
	free((*recv_window)->slider);

	free(*(recv_window));
	*recv_window = NULL;
}


