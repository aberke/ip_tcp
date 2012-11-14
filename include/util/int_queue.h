// exactly Neil's queue.h but storing integers instead of pointers to data -- YOU STILL OWN IT NEIL

#ifndef __INT_QUEUE_H__ 
#define __INT_QUEUE_H__

#include "utils.h"

#define EMPTY_QUEUE -12121212 //for queue.c if queue is empty we return NULL, so we need an identifier for empty for ints

typedef struct int_queue* int_queue_t;

int_queue_t int_queue_init();
void int_queue_destroy(int_queue_t* queue);

void int_queue_set_size(int_queue_t q, int size);
int int_queue_full(int_queue_t q);
int int_queue_pop(int_queue_t q);
int int_queue_peek(int_queue_t q);
int int_queue_push(int_queue_t q, int data);
int int_queue_push_front(int_queue_t q, int data);

#endif // __INT_QUEUE_H__
