#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "list.h"
#include "utils.h"

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

void* plain_list_pop(plain_list_t list){
	pthread_mutex_lock(&(list->lock));
	plain_list_el_t el = list->head;
	if(!el)		
		return NULL;
	else {
		if(el->next) el->next->prev = NULL;
		list->head = el->next;
		list->length--;
		return el;
	}
	pthread_mutex_lock(&(list->lock));
}


void plain_list_remove(plain_list_t list, plain_list_el_t el){
	pthread_mutex_lock(&(list->lock));
	list->length--;
	if(!el->prev){
		list->head = el->next;
		free(el);
	}
	else{
		el->prev->next = el->next;
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
