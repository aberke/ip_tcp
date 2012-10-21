#include <stdlib.h>

#include "queue.h"
#include "window.h"
#include "utils.h"

struct window{
	queue_t data_queue;
	queue_t to_send;	
	destructor_f destructor;
	
	int window_size;
	int left;
	int right;

	void* sliding_window[];
	void* 
};

window_t window_init(/* ARGUMENTS */ ){
	window_t window = (struct window*)malloc(sizeof(struct window));
	/* INITIALIZE FIELDS HERE */

	return window;
}

void window_destroy(window_t* window){
	/* DESTROY FIELDS */

	free(*(window));
	*window = NULL;
}


