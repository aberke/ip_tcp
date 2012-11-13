#ifndef _IP_LIST_H_
#define _IP_LIST_H_

#include <stddef.h>
#include <pthread.h>


typedef struct plain_list_el{	
	void* data;
	struct plain_list_el* prev;
	struct plain_list_el* next;
}* plain_list_el_t;

typedef struct plain_list{
	plain_list_el_t head;
	int length;
	pthread_mutex_t lock;
}* plain_list_t;

plain_list_t plain_list_init();
void plain_list_destroy(plain_list_t* list);
void plain_list_destroy_total(plain_list_t* list, void (*destroy)(void**));
void plain_list_append(plain_list_t list, void* data);
void plain_list_insert_before(plain_list_t list, plain_list_el_t el, void* data);
void plain_list_insert_after(plain_list_t list, plain_list_el_t el, void* data);
void plain_list_remove(plain_list_t list, plain_list_el_t el);
plain_list_el_t plain_list_pop(plain_list_t list);

#define PLAIN_LIST_ITER(list,el)				\
do{												\
	plain_list_el_t __next__;						\
	for(el=((list)->head);el!=NULL;el=__next__){	\
		__next__=el->next;

#define PLAIN_LIST_ITER_DONE(list)				\
	}											\
}												\
while(0)

/// sorted list

typedef struct sorted_list* sorted_list_t;

sorted_list_t sorted_list_init(int (*comparator)(void*, void*));
void sorted_list_destroy(sorted_list_t* s_list);
void sorted_list_destroy_total(sorted_list_t* s_list, void (*destructor) (void**));
void sorted_list_insert(sorted_list_t s_list, void* data);
void* sorted_list_peek(sorted_list_t s_list);
void* sorted_list_pop(sorted_list_t s_list);
plain_list_t sorted_list_get_list(sorted_list_t s_list);

/**
 * This list provides a minimal set of functionality as needed
 * by parselinks. If you'd like to extend this list, feel free
 * to do so.
 *
 * To traverse the list:
 *   node_t *curr;
 *   for (curr = list->head; curr != NULL; curr = curr->next) {
 *     // Do something with 'curr'
 *   }
 *
 */

typedef struct node_t {
    void *data;
    struct node_t *next;
} node_t;

typedef struct iplist_t {
	int length;
	node_t *head;
} iplist_t;

/**
 * Allocates memory for the list and does any necessary setup.
 * The user is responsible for freeing the memory by calling
 * list_free, below.
 */
void iplist_init(iplist_t **list);

/** 
 * Frees all memory explicitly allocated by the list and sets the 
 * pointer to null.
 */
void iplist_free(iplist_t **list);

/**
 * Inserts a new node holding the data at the end of the list (make sure
 * the data is malloced).
 */
void iplist_append(iplist_t *list, void *data);

/** 
 * Returns 1 if the list is empty, 0 if not.
 */
int iplist_empty(iplist_t *list);

#endif
