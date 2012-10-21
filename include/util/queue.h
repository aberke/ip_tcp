#ifndef __QUEUE_H__ 
#define __QUEUE_H__

typedef struct queue* queue_t;

queue_t queue_init();
void queue_destroy(queue_t* queue);

void* queue_pop(queue_t q);
void queue_push(queue_t q);

#endif // __QUEUE_H__
