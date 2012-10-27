#include <stdlib.h>
#include <stdio.h>

#include "queue.h"
#include "utils.h"

struct queue_el{
	void* data;
	struct queue_el* next;
};

struct queue{
	struct queue_el* head;
	struct queue_el* tail;
	int size;
	int length;
};

typedef struct queue_el* queue_el_t;

/* takes the pointers to the next and prev */
queue_el_t queue_el_init(void* data){
	queue_el_t el = malloc(sizeof(struct queue_el));
	el->data = data;
	el->next = NULL;
	return el;
}

/* just free's it */
void queue_el_destroy(queue_el_t* el){
	free(*el);
	*el = NULL;
}


/* inits the queue, will initially be empty */
queue_t queue_init(){
	queue_t q = malloc(sizeof(struct queue));
	q->head = q->tail = NULL;
	q->size = -1;
	q->length = 0;
	return q;
}

void queue_set_size(queue_t q, int size){
	q->size = size;
}

int queue_full(queue_t q){
	return q->size >= 0 && q->length >= q->size;
}

/* 
returns 
	-1  unable to push
	 0  successful
*/
int queue_push(queue_t q, void* data){
	if(q->size >= 0 && q->length >= q->size)	
		return -1;

	queue_el_t tail = q->tail;
	if(!tail){
		q->head = q->tail = queue_el_init(data);
	}
	else{
		tail->next = queue_el_init(data);
		q->tail = tail->next; 
	}

	q->length++;
	return 0;
}

/*
returns
	-1  unable to push
	 0  successful
*/
int queue_push_front(queue_t q, void* data){
	if(q->size >= 0 && q->length >= q->size)	
		return -1;

	queue_el_t head = q->head;
	if(!head){
		q->head = q->tail = queue_el_init(data);
	}
	else{
		q->head = queue_el_init(data);
		q->head->next = head;
	}
	
	q->length++;
	return 0;
}

/* Peek at the head of the queue, but don't remove it */
void* queue_peek(queue_t q){
	if( q->head == NULL ){
		return NULL;
	}

	return q->head->data;
}

/* pop the queue, ie remove the head */
void* queue_pop(queue_t q){
	if(q->head == NULL){
		return NULL;	
	}

	queue_el_t tmp = q->head;
	if (tmp == q->tail)
		q->tail = NULL;

	q->head = tmp->next;
	
	void* result = tmp->data;
	queue_el_destroy(&tmp);	
	q->length--;
	return result;
}

void queue_destroy_total(queue_t* queue, destructor_f destructor){
	queue_el_t tmp, el = (*queue)->head;
	while(el){
		if(destructor)
			destructor(&(el->data));
		tmp = el->next;	
		queue_el_destroy(&(el));
		el = tmp;
	}

	free(*(queue));
	*queue = NULL;
}

void queue_destroy(queue_t* queue){
	queue_el_t tmp, el = (*queue)->head;
	while(el){
		tmp = el->next;	
		queue_el_destroy(&(el));
		el = tmp;
	}

	free(*(queue));
	*queue = NULL;
}


