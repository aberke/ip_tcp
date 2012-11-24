#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "list.h"
#include "utils.h"

////////////////// SORTED LIST ////////////////////////////

struct sorted_list{
	plain_list_t list;
	int (*cmp)(void*, void*);
};

sorted_list_t sorted_list_init(comparator_f cmp){
	sorted_list_t s_list = malloc(sizeof(struct sorted_list));
	s_list->cmp = cmp;
	s_list->list = plain_list_init();
	return s_list;
}

void sorted_list_destroy(sorted_list_t* s_list){
	plain_list_destroy(&((*s_list)->list));
	free(*s_list);
	*s_list=NULL;
}

void sorted_list_destroy_total(sorted_list_t* s_list, destructor_f destructor){
	plain_list_destroy_total(&((*s_list)->list), destructor);
	free(*s_list);
	*s_list=NULL;
}

void* sorted_list_peek(sorted_list_t s_list){
	if(s_list->list->head == NULL)
		return NULL;
	else
		return s_list->list->head->data; // hmmm
}

void* sorted_list_pop(sorted_list_t s_list){
	plain_list_el_t el = plain_list_pop(s_list->list);
	void* data = el->data;
	free(el);
	return data;
}

plain_list_t sorted_list_get_list(sorted_list_t s_list){ 
	return s_list->list;
}

void sorted_list_insert(sorted_list_t s_list, void* data){
	plain_list_el_t el, next;
	void* inList;
	PLAIN_LIST_ITER(s_list->list, el)
		inList = el->data;
		next = el->next;
		if(s_list->cmp(inList, data) > 0){
			plain_list_insert_before(s_list->list, el, data);
			return;
		}
		if(!next){
			plain_list_insert_after(s_list->list, el, data);
			return;
		}
	PLAIN_LIST_ITER_DONE(s_list->list);

	// if you've gotten to this point, the list is empty,
	// so just append (prepend) it
	plain_list_append(s_list->list, data);
}

////////////////// PLAIN LIST ////////////////////////////

plain_list_t plain_list_init(){
	plain_list_t l = malloc(sizeof(struct plain_list));
	l->head = NULL;
	l->length = 0;
	pthread_mutex_init(&(l->lock), NULL);
	return l;
}

void plain_list_destroy(plain_list_t* list){
	/* make sure no one else has it */
	pthread_mutex_lock(&((*list)->lock));

	plain_list_el_t tmp, el = (*list)->head;

	while(el){
		tmp = el->next;
		free(el);
		el = tmp;
	}

	free(*list);
	*list = NULL;
}

void plain_list_destroy_total(plain_list_t* list, destructor_f destroy){
	plain_list_el_t tmp, el = (*list)->head;

	while(el){
		tmp = el->next;
		destroy(&(el->data));
		free(el);
		el = tmp;
	}
	
	free(*list);
	*list = NULL;
}

void plain_list_insert_before(plain_list_t list, plain_list_el_t el, void* data){
	pthread_mutex_lock(&(list->lock));
	if(el->prev==NULL){
		plain_list_append(list, data);
	}
	else{
		plain_list_el_t new_el = malloc(sizeof(struct plain_list_el));
		new_el->data = data;
		new_el->prev = el->prev;
		new_el->next = el;
		el->prev->next = new_el;
		el->prev = new_el;
		list->length++;
	}
	pthread_mutex_unlock(&(list->lock));
}

void plain_list_insert_after(plain_list_t list, plain_list_el_t el, void* data){
	pthread_mutex_lock(&(list->lock));

	/* first the easy stuff */
	plain_list_el_t new_el = malloc(sizeof(struct plain_list_el));
	new_el->data = data;
	list->length++;

	/* now find out where it should go */
	if(!el){
		list->head = new_el;
		new_el->prev = NULL;
		new_el->next = NULL;
	}
	else if(el->next){
		new_el->next = el->next;
		el->next = new_el;
		
		new_el->next->prev = new_el;
		new_el->prev = el;
	}
	else{
		new_el->next = NULL;
		el->next = new_el;
		new_el->prev = el;
	}

	pthread_mutex_unlock(&(list->lock));
}

void plain_list_append(plain_list_t list, void* data){
	pthread_mutex_lock(&(list->lock));
	plain_list_el_t tmp = list->head;
	list->head = malloc(sizeof(struct plain_list_el));
	list->head->data = data;
	list->head->prev = NULL;
	list->head->next = tmp;
	if(tmp)
		tmp->prev = list->head;
	list->length++;
	pthread_mutex_unlock(&(list->lock));
}

plain_list_el_t plain_list_pop(plain_list_t list){
	pthread_mutex_lock(&(list->lock));
	plain_list_el_t result, el = list->head;
	if(!el)		
		result = NULL;
	else {
		if(el->next) el->next->prev = NULL;
		list->head = el->next;
		list->length--;
		result = el;
	}
	pthread_mutex_unlock(&(list->lock));
	return result;
}


void plain_list_remove(plain_list_t list, plain_list_el_t el){
	pthread_mutex_lock(&(list->lock));
	list->length--;
	if(!el->prev){
		list->head = el->next;
        if(el->next){
            el->next->prev = NULL;
        }
		free(el);
	}
	else{
		el->prev->next = el->next;
		if(el->next)
			el->next->prev = el->prev;
		free(el);
	}
	pthread_mutex_unlock(&(list->lock));
}

/* OTHER HELPERS */

void iplist_init(iplist_t **list)
{
	*list = (iplist_t *)malloc(sizeof(iplist_t));
	(*list)->length = 0;
	memset(*list, 0, sizeof(iplist_t));
}

void iplist_free(iplist_t **list)
{
	if ((*list) != NULL) { 
		node_t *curr, *next;
		for (curr = (*list)->head; curr != NULL; curr = next) {
			next = curr->next;
			free(curr);
		}
		free(*list);
		*list = NULL;
	}
}

void iplist_append(iplist_t *list, void *data)
{
	node_t *new_node = (node_t *)malloc(sizeof(node_t));
	memset(new_node, 0, sizeof(node_t));
	new_node->data = data;
	
	if (iplist_empty(list)) {
		list->head = new_node;
	} else {
		node_t *curr;
		for (curr = list->head; curr->next != NULL; curr = curr->next);
		curr->next = new_node;
	}	
	list->length++;
}

int iplist_empty (iplist_t *list)
{
	return list->head == NULL;
}
