#include <stdlib.h>

#include "queue.h"

struct queue_el{
	void* data;
	struct queue_el* next;
};

struct queue{
	struct queue_el* head;
	struct queue_el* tail;
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
	return q;
}

void queue_push(queue_t q, void* data){
	queue_el_t tail = q->tail;
	if(!tail){
		q->head = q->tail = queue_el_init(data);
	}
	else{
		tail->next = queue_el_init(data);
		q->tail = tail->next; 
	}
}

void* queue_pop(queue_t q){
	if(q->head == NULL){
		puts("ERROR POPPING FROM EMPTY QUEUE!!");
		return NULL;	
	}

	queue_el_t tmp = q->head;
	if (tmp == q->tail)
		q->tail = NULL;

	q->head = tmp->next;
	
	void* result = tmp->data;
	queue_el_destroy(&tmp);	
	return result;
}

void queue_destroy(queue_t* queue){
	queue_el_t tmp, el = queue->head;
	while(el){
		tmp = el->next;	
		queue_el_destroy(&(el));
		el = tmp;
	}

	free(*(queue));
	*queue = NULL;
}


