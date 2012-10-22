#include <stdlib.h>

#include "queue.h"

struct queue_el{
	void* data;
	struct queue_el* next;
	struct queue_el* prev;
};

struct queue{
	struct queue_el* head;
	struct queue_el* tail;
};

typedef struct queue_el* queue_el_t;

queue_el_t queue_el_init(queue_el_t prev, void* data){
	queue_el_t el = malloc(sizeof(struct queue_el));
	el->data = data;
	el->next = NULL;
	el->prev = prev;
	return el;
}

void queue_el_destroy(queue_el_t* el){
	if(el->next){
		queue_el_destroy(el->next);
	}

	free(*el);
	*el = NULL;
}

queue_t queue_init(){
	queue_t q = malloc(sizeof(struct queue));
	q->head = q->tail = NULL;
	return q;
}

void queue_push(queue_t q, void* data){
	queue_el_t tmp = q->head;
	q->head = queue_el_init(data);
	q->head->next = tmp;
}

void* queue_pop(queue_t q){
	queue_el_t tmp = q->head;
	q->head = tmp->next;
	
	void* result = tmp->data;
	queue_el_destroy(&tmp);	
	return result;
}

void queue_destroy(queue_t* queue){
	if(queue->head){
		queue_el_destroy(&(queue->head));
	}

	free(*(queue));
	*queue = NULL;
}


