#include <inttypes.h>

/* Alex needs: */
#include <netinet/ip.h>
/***************/

#include "ip_node.h"
#include "forwarding_table.h"
#include "routing_table.h"
#include "link_interface.h"
#include "util/parselinks.h"
#include "util/list.h"
#include "util/utils.h"

//// select
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

//// hash-map 
#include "uthash.h"


//// some helpful static globals
#define STDIN fileno(stdin)
#define SELECT_TIMEOUT 1

/*for ip packet*/
#define RIP_DATA 200  
#define TEST_DATA 0   

/* Static functions for internal use */
static void _update_select_list(ip_node_t node);
static void _handle_selected(ip_node_t node, link_interface_t interface);
static void _handle_reading_sockets(ip_node_t node);
static void _handle_user_command(ip_node_t node);

/* STRUCTS */

/* uthash works by keying on a field of a struct, and using that key as 
   the hashmap key, therefore, it would be impossible to create two hashmaps
   just using the link_interface struct without duplicating each of the 
   structs. The solution is to create structs that extract the keyed field. 
   We have two of these in order to have hashmaps that map the socket, and the ip, 
   and they are named accordingly */

struct interface_socket_keyed{
	link_interface_t interface;
	int socket;

	UT_hash_handle hh;
};

struct interface_ip_keyed{
	link_interface_t interface;
	uint32_t ip;

	UT_hash_handle hh;
};

typedef struct interface_socket_keyed* interface_socket_keyed_t;
typedef struct interface_ip_keyed* interface_ip_keyed_t;


/* The ip_node has a forwarding_table, a routing_table and then the number of
   interfaces that it owns, an array to keep them in, and then a hashmap that maps
   sockets/ip addresses to one of these interface pointers. The ip_node also needs
   an fdset in order to use select() for reading from each of the interfaces, as 
   well as a highsock that will be passed to select(). */

struct ip_node{
	forwarding_table_t forwarding_table;
	routing_table_t routing_table;
	int num_interfaces;
	link_interface_t* interfaces;
	interface_socket_keyed_t socketToInterface;
	interface_ip_keyed_t addressToInterface;

	fd_set read_fds;
	int highsock;

	int running; 
};	

/* CTORS/DTORS */

/* These are straightforward in the case of the keyed structs. Just store the interface
   and pull out the socket/ip_address */

interface_socket_keyed_t interface_socket_keyed_init(link_interface_t interface){
	interface_socket_keyed_t sock_keyed = malloc(sizeof(struct interface_socket_keyed));
	sock_keyed->socket = link_interface_get_sfd(interface);
	sock_keyed->interface = interface;
	return sock_keyed;
}

//// DO NOT DESTROY THE INTERFACE
void interface_socket_keyed_destroy(interface_socket_keyed_t* sock_keyed){
	free(*sock_keyed);
	*sock_keyed = NULL;
}


interface_ip_keyed_t interface_ip_keyed_init(link_interface_t interface){
	interface_ip_keyed_t ip_keyed = malloc(sizeof(struct interface_ip_keyed));
	ip_keyed->ip = link_interface_get_local_virt_ip(interface);
	ip_keyed->interface = interface;
	
	return ip_keyed;
}

//// DO NOT DESTROY THE INTERFACE
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
	
	ip_node->interfaces = (link_interface_t*)malloc(sizeof(link_interface_t)*(ip_node->num_interfaces));
	ip_node->socketToInterface = NULL;
	ip_node->addressToInterface = NULL;
	
	link_interface_t interface; 
	link_t* link;
	node_t* curr;
	interface_socket_keyed_t socket_keyed;
	interface_ip_keyed_t ip_keyed;	

	/* keep track of the index in order to populate
       the array of interfaces */
	int index=0;
	for(curr = links->head; curr != NULL; curr = curr->next){
		link = (link_t*)curr->data;
		if((interface = link_interface_create(link)) == NULL){
			//todo: add error handing for when socket doesn't bind
			puts("link_interface didn't init properly");
		}
		ip_node->interfaces[index] = interface;
	
		/* Now add each interface to the hashmaps. In order to do this we use the macro provided 
		   by uthash which takes in the hash-map (just an array of structs that should be initialized
	       to null), the name of the field that will be used as the key, and then the struct that 
           contains the key/info */
		
		socket_keyed = interface_socket_keyed_init(interface);
		ip_keyed     = interface_ip_keyed_init(interface);

		HASH_ADD_INT(ip_node->socketToInterface, socket, socket_keyed);
		HASH_ADD_INT(ip_node->addressToInterface, ip, ip_keyed);

		index++;
	}	 

	//// you're still running right? right
	ip_node->running = 1;

	//// clean up -- free the list of links and free all of the links
	free_links(links);

	return ip_node;
}

void ip_node_destroy(ip_node_t* ip_node){
	//// destroy forwarding/routing tables
	forwarding_table_destroy(&((*ip_node)->forwarding_table));
	routing_table_destroy(&((*ip_node)->routing_table));
	
	//// iterate through the hash maps and destroy all of the keys/values,
	//// this will NOT destroy the interfaces
	interface_socket_keyed_t socket_keyed, tmp_sock_keyed;
	HASH_ITER(hh, (*ip_node)->socketToInterface, socket_keyed, tmp_sock_keyed){
		puts("destroying..."); 
		HASH_DEL((*ip_node)->socketToInterface, socket_keyed);
		interface_socket_keyed_destroy(&socket_keyed);
	}

	//// ditto (see above)
	interface_ip_keyed_t ip_keyed, tmp_ip_keyed;
	HASH_ITER(hh, (*ip_node)->addressToInterface, ip_keyed, tmp_ip_keyed){
		HASH_DEL((*ip_node)->addressToInterface, ip_keyed);
		interface_ip_keyed_destroy(&ip_keyed);
	}

	//// NOW destroy all the interfaces
	int i;
	for(i=0;i<(*ip_node)->num_interfaces;i++){
		link_interface_destroy((*ip_node)->interfaces[i]);
	}		
	free((*ip_node)->interfaces);

	//// basic clean up
	free(*ip_node);
	*ip_node = NULL;

	//// and we're done!
}

///// PRINTERS
void ip_node_print_interfaces(ip_node_t ip_node){
	int i;
	for(i=0;i<ip_node->num_interfaces;i++){
		link_interface_print(ip_node->interfaces[i]);
	}
}



/* ip_node_start will take just the ip_node as a parameter and will start
   up the whole process of listening to all the interfaces, and handling all
   of the information */
void ip_node_start(ip_node_t ip_node){
	int retval;

	//// init the timeval struct for breaking out of select
	struct timeval tv;
	tv.tv_sec = SELECT_TIMEOUT;
	tv.tv_usec = 0;	

	while(ip_node->running){
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
static void _handle_reading_sockets(ip_node_t ip_node){
	//// if there's incoming data from the user, pass off to _handle_user_command
	if(FD_ISSET(STDIN, &(ip_node->read_fds))){
		_handle_user_command(ip_node);
	}

	struct interface_socket_keyed *socket_keyed, *tmp;
	HASH_ITER(hh, ip_node->socketToInterface, socket_keyed, tmp){
		if(FD_ISSET(socket_keyed->socket, &(ip_node->read_fds))){
			_handle_selected(ip_node, socket_keyed->interface);	
		}	
	}
}

/* This will handle updating the fdset that the ip_node uses in order
   to read from each of the interfaces. It checks that each interface
   is up before adding it (#DESIGN-DECISION) */ 

static void _update_select_list(ip_node_t ip_node){
	FD_ZERO(&(ip_node->read_fds));
	FD_SET(STDIN, &(ip_node->read_fds));
	int max_fd = fileno(stdin);	
	int sfd;		
	
	int i;
	for(i=0;i<ip_node->num_interfaces;i++){
		sfd = link_interface_get_sfd(ip_node->interfaces[i]);
		max_fd = (sfd > max_fd ? sfd : max_fd);
		FD_SET(sfd, &(ip_node->read_fds));
	}
	ip_node->highsock = max_fd;
}

/* _handle_user_command does exactly what it says. Note: this is only called 
   if reading from STDIN won't block, so just do it already. */

static void _handle_user_command(ip_node_t ip_node){
	char* buffer = (char*) malloc(sizeof(char)*BUFFER_SIZE);
	fgets(buffer, BUFFER_SIZE-1, stdin);
	rtrim(buffer, "\n");
	
	//// handle the commands
	if(!strcmp(buffer, "quit") || !strcmp(buffer, "q"))
		ip_node->running = 0;
	
	else if(!strcmp(buffer, "interfaces"))
		ip_node_print_interfaces(ip_node);

	else
		printf("Received unrecognized input from user: %s\n", buffer); 
	
	free(buffer); 
}

/* _handle_selected is a dummy function for testing the functionality of the rest
   of the system (and not the implementation of the link_interface). This will be 
   done by linking to a dummy link_interface file that provides the same methods */
static void _handle_selected(ip_node_t ip_node, link_interface_t interface){
	puts("Handling selected."); 
}

