#ifndef __QUEUE_H__ 
#define __QUEUE_H__

#include "utils.h"

typedef struct queue* queue_t;

queue_t queue_init();
void queue_destroy(queue_t* queue);
void queue_destroy_total(queue_t* queue, destructor_f destructor);

void* queue_pop(queue_t q);
void* queue_peek(queue_t q);
void queue_push(queue_t q, void* data);

#endif // __QUEUE_H__
