#include <inttypes.h>

#include "uthash.h"
#include "list.h"

/* STRUCTS */

struct interface_socket_keyed{
	struct link_interface* interface;
	int socket;
};

struct interface_ip_keyed{
	struct link_interface* interface;
	int ip;
};

struct node{
	forwarding_table_t forwarding_table;
	routing_table_t routing_table;
	int num_interfaces;
	struct link_interface* interfaces;
	struct interface_socket_keyed *socketToInterface;
	struct interface_ip_keyed *addressToInterface;
};	

typedef struct interface_socket_keyed* interface_socket_keyed_t;
typedef struct interface_ip_keyed* interface_ip_keyed_t;

/* CTORS/DTORS */

interface_socket_keyed interface_socket_keyed_init(int socket, link_interface_t interface){
	interface_socket_keyed_t sock_keyed = malloc(sizeof(struct interface_socket_keyed));
	sock_keyed->socket = socket;
	sock_keyed->interface = interface;
	return sock_keyed;
}

void interface_socket_keyed_destroy(interfacce_socket_keyed_t* sock_keyed){
	free(*sock_keyed);
	*sock_keyed = NULL;
}

interface_ip_keyed_t interface_ip_keyed_init(uint32_t ip, link_interface_t interface){
	interface_ip_keyed_t ip_keyed = malloc(sizeof(struct interface_ip_keyed));
	ip_keyed->ip = ip;
	ip_keyed->interface = interface;
}

void interface_ip_keyed_destroy(interface_ip_keyed_t* ip_keyed){
	free(*ip_keyed);
	*ip_keyed = NULL;
}

node_t node_init(list_t* interfaces){
	node_t node = (node_t)malloc(sizeof(struct node));
	node->forwarding_table = forwarding_table_init();
	node->routing_table = routing_table_init();
	node->num_interfaces = interfaces->length;	
	node->interfaces = (struct link_interface*)malloc(sizeof(struct link_interface)*node->num_interfaces);
	
	link_interface_t interface; 
	int interfact_socket;
	uint32_t interface_ip;
	node_t* curr;

	int index=0;
	for(curr = interfaces->head; curr != NULL; curr = curr->next){
		interface = (link_interface_t)curr->data;		
		interface_socket = interface_get_socket(interface);
		
		interface_ip 	 = interface_get_ip(interface);
		node->interfaces[index] = interface;
		HASH_ADD_INT(node->socketToInterface, socket, interface_socket_keyed_init(interface_socket, interface));
		HASH_ADD_INT(node->addressToInterface, ip, interface_ip_keyed_init(interface_ip, interface));
		index++;
	}	 

	return node;
}



void handle_selected(


