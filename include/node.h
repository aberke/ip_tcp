#ifndef __NODE_H__
#define __NODE_H__

#include "list.h"

typedef struct node* node_t; 

node_t node_init(link_t* link);
void node_destroy(node_t* node);

void node_start();
void node_stop();


#endif // __NODE_H__
