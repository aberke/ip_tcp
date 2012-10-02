#include <inttypes.h>

/* Alex needs: */
#include <netinet/ip.h>
#include "util/ipsum.h"
/***************/

#include "uthash.h"
#include "parselinks.h"
#include "list.h"
#include "utils.h"
#include "link_interface.h"

#define STDIN fileno(stdin)
#define SELECT_TIMEOUT 1

/*for ip packet*/
#define RIP_DATA 200  
#define TEST_DATA 0   

/* Static functions for internal use */
static void _update_select_list(ip_node_t node);
static void _handle_selected(ip_node_t node, link_interface_t interface);
static void _handle_reading_sockets(ip_node_t node);

/* STRUCTS */

/* uthash works by keying on a field of a struct, and using that key as 
   the hashmap key, therefore, it would be impossible to create two hashmaps
   just using the link_interface struct without duplicating each of the 
   structs. The solution is to create structs that extract the keyed field. 
   We have two of these in order to have hashmaps that map the socket, and the ip, 
   and they are named accordingly */

struct interface_socket_keyed{
	struct link_interface* interface;
	int socket;
};

struct interface_ip_keyed{
	struct link_interface* interface;
	int ip;
};

/* The ip_node has a forwarding_table, a routing_table and then the number of
   interfaces that it owns, an array to keep them in, and then a hashmap that maps
   sockets/ip addresses to one of these interface pointers. The ip_node also needs
   an fdset in order to use select() for reading from each of the interfaces, as 
   well as a highsock that will be passed to select(). */

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

/* These are straightforward in the case of the keyed structs. Just store the interface
   and pull out the socket/ip_address */

interface_socket_keyed interface_socket_keyed_init(link_interface_t interface){
	interface_socket_keyed_t sock_keyed = malloc(sizeof(struct interface_socket_keyed));
	sock_keyed->socket = link_interface_get_socket(interface);
	sock_keyed->interface = interface;
	return sock_keyed;
}

void interface_socket_keyed_destroy(interfacce_socket_keyed_t* sock_keyed){
	free(*sock_keyed);
	*sock_keyed = NULL;
}


interface_ip_keyed_t interface_ip_keyed_init(link_interface_t interface){
	interface_ip_keyed_t ip_keyed = malloc(sizeof(struct interface_ip_keyed));
	ip_keyed->ip = link_interface_get_ip(ip);
	ip_keyed->interface = interface;
}

void interface_ip_keyed_destroy(interface_ip_keyed_t* ip_keyed){
	free(*ip_keyed);
	*ip_keyed = NULL;
}

/* ip_node_init is a little more involved. It takes in a list of links, and iterates
   through these in order to populate its fields. It simply extracts each interface and puts
   it in the ip_node's interface array, and also adds each interface to both hash-maps. */

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
	
	/* keep track of the index in order to populate
       the array of interfaces */
	int index=0;
	for(curr = links->head; curr != NULL; curr = curr->next){
		link = (link_t)curr->data;
		if((interface = link_interface_create(link)) == NULL){
			//todo: add error handing for when socket doesn't bind
		}
		ip_node->interfaces[index] = interface;

		/* Now add each interface to the hashmaps. In order to do this we use the macro provided 
		   by uthash which takes in the hash-map (just an array of structs that should be initialized
	       to null), the name of the field that will be used as the key, and then the struct that 
           contains the key/info */
		HASH_ADD_INT(ip_node->socketToInterface, socket, interface_socket_keyed_init(interface));
		HASH_ADD_INT(ip_node->addressToInterface, ip, interface_ip_keyed_init(interface));
		index++;
	}	 

	return ip_node;
}

/******************** ALEX's AREA ************************/

/*KEEP IN MIND:
"We ask you to design and implement an interface that allows an upper layer to register a handler
for a given protocol number. We’ll leave its speciﬁcs up to you."
*/

/*"When IP packets arrive at their destination, if they aren’t RIP packets, you should
simply print them out in a useful way."*/
void handle_local_packet(link_interface_t li, void* packet, int packet_len){
	//print packet nicely
	// future use will be to hand packet to tcp
}

void handle_selected(link_interface_t li){
	
	//must hand read_packet(link_interface, buffer, buffer_len) a buffer
	char buffer[IP_PACKET_MAX_SIZE];
	memset (buffer, 0, IP_MAX_SIZE);
	int bytes_read;
	bytes_read = read_packet(li, buffer, IP_PACKET_MAX_SIZE);
	char unwrapped[bytes_read];
	int protocol;
	protocol = unwrap_ip_packet(buffer, bytes_read, unwrapped);

}
//write wrap_ip_packet //don't have to deal with fragmentation but make sure you don't send more than limit


//write unwrap_ip_packet
/* use:
IP checksum calculation: ipsum.c ipsum.h. Use this function to calculate the checksum in
the IP header for you.
*/
// int is type: RIP vs other  --return -1 if bad packet
// fills unwrapped with unwrapped packet
int unwrap_ip_packet(void* packet, int packet_len, char* unwrapped){
	if(packet_len < sizeof(struct ip)){
		//packet not large enough
		puts("received packet with packet_len < sizeof(struct ip)");
		return -1;
	}

	u_int header_length;
	u_short ip_len, ip_sum;
	u_char protocol;
	struct  in_addr src_ip, dest_ip;
	
	char header[sizeof(struct ip)];
	memcpy(header,  = packet;
	
	struct ip *ip_header = (struct ip *)packet;
	ip_sum = ip_header->ip_sum;
	if(ip_sum != ip_sum(header, sizeof)
	
	ip_len = ip_header->ip_len;
	
	char unwrapped[ip_len];
	unwrapped = buffer[4*(ip_header->ip_hl)];
	
	


	return 0;
}




/******************* END OF ALEX's AREA *************************/

/* ip_node_start will take just the ip_node as a parameter and will start
   up the whole process of listening to all the interfaces, and handling all
   of the information */
void ip_node_start(ip_node_t ip_node){
	int retval;

	//// init the timeval struct for breaking out of select
	struct timeval tv;
	tv.tv_sec = SELECT_TIMEOUT;
	tv.tv_usec = 0;	

	while(1){
		//// first update the list (rebuild it)
		_update_select_list(ip_node);

		//// make sure you didn't error out, otherwise pass off to _handle_reading_sockets
		retval = select(ip_node->highsock + 1, &(ip_node->read_fds), NULL, NULL, &tv);
		if (retval == -1)
			{ error("select()"); } // #DESIGN-DECISION 	
		else if (retval)	
			_handle_reading_sockets(ip_node);
	}
}

/* _handle_reading_sockets is an internal function for dealing with the 
   effect of a select call. This will get called only if there is a fd to
   read from. First check STDIN, and handle that command. Then check all 
   of the interface sockets by iterating over the hashmap of sockets to interfaces. */
void _handle_reading_sockets(ip_node_t ip_node){
	//// if there's incoming data from the user, pass off to _handle_user_command
	if(FD_ISSET(STDIN, &(ip_node->read_fds)))
		_handle_user_command(ip_node);

	struct interface_socket_keyed socket_keyed, tmp;
	HASH_ITER(hh, ip_node->socketToInterface, socket_keyed, tmp){
		if(FD_ISSET(socket_keyed.socket, &(ip_node->read_fds)))
			_handle_selected(ip_node, socket_keyed.interface);		
	}
}

/* This will handle updating the fdset that the ip_node uses in order
   to read from each of the interfaces. It checks that each interface
   is up before adding it (#DESIGN-DECISION) */ 

void _update_select_list(ip_node_t ip_node){
	FD_ZERO(&(ip_node->readfds));
	FD_SET(STDIN, &(ip_node->readfds));
	int max_fd = fileno(stdin);	
	int sfd;		
	
	int i;
	for(i=0;i<ip_node->num_interfaces;i++){
		sfd = interface_get_socket(ip_node->interfaces[i]);
		max_fd = (sfd > max_fd ? sfd : max_fd);
		FD_SET(sfd, &(ip_node->readfds));
	}
	ip_node->highsock = max_fd;
}

/* _handle_selected is a dummy function for testing the functionality of the rest
   of the system (and not the implementation of the link_interface). This will be 
   done by linking to a dummy link_interface file that provides the same methods */
void _handle_selected(ip_node_t ip_node, link_interface_t interface){
	puts("Handling selected."); 
}

