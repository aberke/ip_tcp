#ifndef __NODE_H__
#define __NODE_H__

typedef struct node* node_t; 

node_t node_init();
void node_destroy(node_t*);

void node_start();
void node_stop();


#endif // __NODE_H__
