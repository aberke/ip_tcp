#include <inttypes.h>

#include "uthash.h"
#include "parselinks.h"
#include "list.h"
#include "link_interface.h"

#define STDIN fileno(stdin)

/* This is
in order to test git
*/

/* STRUCTS */

struct interface_socket_keyed{
	struct link_interface* interface;
	int socket;
};

struct interface_ip_keyed{
	struct link_interface* interface;
	int ip;
};

struct ip_node{
	forwarding_table_t forwarding_table;
	routing_table_t routing_table;
	int num_interfaces;
	struct link_interface* interfaces;
	struct interface_socket_keyed *socketToInterface;
	struct interface_ip_keyed *addressToInterface;

	fd_set read_fds;
	int highsock;
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

ip_node_t ip_node_init(list_t* links){
	ip_node_t ip_node = (ip_node_t)malloc(sizeof(struct ip_node));
	ip_node->forwarding_table = forwarding_table_init();
	ip_node->routing_table = routing_table_init();
	ip_node->num_interfaces = links->length;	
	ip_node->interfaces = (struct link_interface*)malloc(sizeof(struct link_interface)((*ip_node)->num_interfaces));
	
	link_interface_t interface; 
	link_t link;
	int interfact_socket;
	uint32_t interface_ip;
	node_t* curr;

	int index=0;
	for(curr = links->head; curr != NULL; curr = curr->next){
		link = (link_t)curr->data;
		if((interface = link_interface_create(link)) == NULL){
			//todo: add error handing for when socket doesn't bind
		}
		interface_socket = interface_get_socket(interface);
		interface_ip 	 = interface_get_ip(interface);
		ip_node->interfaces[index] = interface;
		HASH_ADD_INT(ip_node->socketToInterface, socket, interface_socket_keyed_init(interface_socket, interface));
		HASH_ADD_INT(ip_node->addressToInterface, ip, interface_ip_keyed_init(interface_ip, interface));
		index++;
	}	 

	return ip_node;
}

void ip_node_update_select_list(ip_node_t ip_node){
	FD_ZERO(&(ip_node->readfds));
	FD_SET(&(ip_node->readfds), fileno(stdin));
	int max_fd = fileno(stdin);	
	int sfd;		
	
	int i;
	for(i=0;i<ip_node->num_interfaces;i++){
		sfd = interface_get_socket(ip_node);
		max_fd = (sfd > max_fd ? sfd : max_fd);
		FD_SET(&(ip_node->readfds), sfd);
	}
	ip_node->highsock = max_fd;
}

/******************** ALEX's AREA ************************/

void handle_selected(link_interface_t li){

	char buffer[IP_PACKET_MAX_SIZE] = {0};

}
//write wrap_ip_packet //don't have to deal with fragmentation but make sure you don't send more than limit


//write unwrap_ip_packet







/******************* END OF ALEX's AREA *************************/


/****************INTERNAL FUNCTIONS******************/
static void _update_fd_sets(ip_node_t ip_node){
	FD_ZERO(&(ip_node->read_fds));
	FD_SET(STDIN, &(ip_node->read_fds));
	
	int i;
	link_interface_t interface;
	for(i=0;i<ip_node->num_interfaces;i++){
		interface = ip_node->interfaces[i];
		if(link_interface_is_up(interface))
			FD_SET(link_interface_get_socket(interface), &(ip_node->read_fds));
	}
}
/* hello there */
/* another test */
