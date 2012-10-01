#include "uthash.h"
#include "list.h"

struct interface_socket_keyed{
	struct link_interface* interface;
	int socket;
};

struct interface_ip_keyed{
	struct link_interface* interface;
	int socket;
};

struct node{
	forwarding_table_t forwarding_table;
	routing_table_t routing_table;
	int num_interfaces;
	struct link_interface* interfaces;
	struct interface_socket_keyed *socketToInterface;
	struct interface_ip_keyed *addressToInterface;
};	

node_t node_init(list_t* interfaces){
	node_t node = (node_t)malloc(sizeof(struct node));
	node->forwarding_table = forwarding_table_init();
	node->routing_table = routing_table_init();
	node->num_interfaces = interfaces->length;	
	
	node_t* curr;
	for(curr = interfaces->head; curr != NULL; curr = curr->next){
		
	}	 
