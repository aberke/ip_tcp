#ifndef __IP_NODE_H__
#define __IP_NODE_H__

#include "util/list.h"

typedef struct ip_node* ip_node_t; 

ip_node_t ip_node_init(list_t* links);
void ip_node_destroy(ip_node_t* ip_node);

void ip_node_start();
void ip_node_stop();


#endif // __IP_NODE_H__
