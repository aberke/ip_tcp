// exactly Neil's queue.c but storing integers instead of pointers to data -- YOU STILL OWN IT NEIL

#include <stdlib.h>
#include <stdio.h>

#include "int_queue.h"
#include "utils.h"

struct int_queue_el{
	int data;
	struct int_queue_el* next;
};

struct int_queue{
	struct int_queue_el* head;
	struct int_queue_el* tail;
	int size;
	int length;
};

typedef struct int_queue_el* int_queue_el_t;

/* takes the pointers to the next and prev */
int_queue_el_t int_queue_el_init(int data){
	int_queue_el_t el = malloc(sizeof(struct int_queue_el));
	el->data = data;
	el->next = NULL;
	return el;
}

/* just free's it */
void int_queue_el_destroy(int_queue_el_t* el){
	free(*el);
	*el = NULL;
}


/* inits the queue, will initially be empty */
int_queue_t int_queue_init(){
	int_queue_t q = malloc(sizeof(struct int_queue));
	q->head = q->tail = NULL;
	q->size = -1;
	q->length = 0;
	return q;
}

void int_queue_set_size(int_queue_t q, int size){
	q->size = size;
}

int int_queue_full(int_queue_t q){
	return q->size >= 0 && q->length >= q->size;
}

/* 
returns 
	-1  unable to push
	 0  successful
*/
int int_queue_push(int_queue_t q, int data){
	if(q->size >= 0 && q->length >= q->size)	
		return -1;

	int_queue_el_t tail = q->tail;
	if(!tail){
		q->head = q->tail = int_queue_el_init(data);
	}
	else{
		tail->next = int_queue_el_init(data);
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
int int_queue_push_front(int_queue_t q, int data){
	if(q->size >= 0 && q->length >= q->size)	
		return -1;

	int_queue_el_t head = q->head;
	if(!head){
		q->head = q->tail = int_queue_el_init(data);
	}
	else{
		q->head = int_queue_el_init(data);
		q->head->next = head;
	}
	
	q->length++;
	return 0;
}

/* Peek at the head of the queue, but don't remove it */
int int_queue_peek(int_queue_t q){
	if( q->head == NULL ){
		return EMPTY_QUEUE;
	}

	return q->head->data;
}

/* pop the queue, ie remove the head */
int int_queue_pop(int_queue_t q){
	if(q->head == NULL){
		return EMPTY_QUEUE;	
	}

	int_queue_el_t tmp = q->head;
	if (tmp == q->tail)
		q->tail = NULL;

	q->head = tmp->next;
	
	int result = tmp->data;
	int_queue_el_destroy(&tmp);	
	q->length--;
	return result;
}


void int_queue_destroy(int_queue_t* queue){
	int_queue_el_t tmp, el = (*queue)->head;
	while(el){
		tmp = el->next;	
		int_queue_el_destroy(&(el));
		el = tmp;
	}

	free(*(queue));
	*queue = NULL;
}
