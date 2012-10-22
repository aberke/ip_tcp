#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "list.h"

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
