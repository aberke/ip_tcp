#include <inttypes.h>
#include <netinet/ip.h>

#include "ip_node.h"
#include "ip_utils.h"
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
#define RIP_COMMAND_REQUEST 1
#define RIP_COMMAND_RESPONSE 2

/* Static functions for internal use */
static void _update_select_list(ip_node_t node);
static void _handle_selected(ip_node_t node, link_interface_t interface);
static void _handle_reading_sockets(ip_node_t node);
static void _handle_user_command(ip_node_t node);
static int _is_local_ip(ip_node_t ip_node, uint32_t ip);
static void _handle_query_interfaces(ip_node_t ip_node);
static void _handle_user_command_down(ip_node_t ip_node, char* buffer);
static void _handle_user_command_send(ip_node_t ip_node, char* buffer);

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
		if((interface = link_interface_create(link, index)) == NULL){
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
	// TODO: send out RIP request message

	while(ip_node->running){
		//// first update the list (rebuild it)
		_update_select_list(ip_node);

		//// make sure you didn't error out, otherwise pass off to _handle_reading_sockets
		retval = select(ip_node->highsock + 1, &(ip_node->read_fds), NULL, NULL, &tv);
		if (retval == -1)
			{ error("select()"); } // #DESIGN-DECISION 	
		else if (retval)	
			_handle_reading_sockets(ip_node);
		
		/* STILL TODO *******
		
		if time elapsed > 5 s:
			update_all_interfaces()	
		**********************/
			
	}
}
/*************************** INTERNAL ******************************/
/* _handle_reading_sockets is an internal function for dealing with the 
   effect of a select call. This will get called only if there is a fd to
   read from. First check STDIN, and handle that command. Then check all 
   of the interface sockets by iterating over the hashmap of sockets to interfaces. */
static void _handle_reading_sockets(ip_node_t ip_node){
	//// if there's incoming data from the user, pass off to _handle_user_command
	if(FD_ISSET(STDIN, &(ip_node->read_fds))){
		_handle_user_command(ip_node);
	}
	// iterate through interfaces to see if any have gone up or down since last checked -- in which case must update routing table
	_handle_query_interfaces(ip_node);

	struct interface_socket_keyed *socket_keyed, *tmp;
	HASH_ITER(hh, ip_node->socketToInterface, socket_keyed, tmp){
		if(FD_ISSET(socket_keyed->socket, &(ip_node->read_fds))){
			_handle_selected(ip_node, socket_keyed->interface);	
		}	
	}
}
/*  Recently added by Alex: _handle_query_interfaces()
	iterates through interfaces to check if any interfaces have gone up or down since last checked.
	link_interface_up_down(link_interface_t l_i) returns 0 if status hasn't changed since function last called
	if status has recently changed: returns -1 if down, returns 1 if up -- in which case we need to update routing_table
	This function handles checking each interface for a status changed and updating the routing table as needed
*/
static void _handle_query_interfaces(ip_node_t ip_node){
	// create routing_info struct to fill with information as iterate through interfaces -- then send info to routing table
	struct routing_info *info = (struct routing_info *)malloc(sizeof(struct routing_info) + sizeof(struct cost_address));
	// command and num_entries will be the same for each interface
	info->command = 2;
	info->num_entries = 1;
	// set up variables to iteration
	uint32_t next_hop_addr;
	struct interface_socket_keyed *socket_keyed, *tmp;
	// iterate through interfaces in hashmap
	HASH_ITER(hh, ip_node->socketToInterface, socket_keyed, tmp){
		link_interface_t interface = socket_keyed->interface;
		// check if interface status on whether up or down has changed since last checked
		int up_down;
		if((up_down = link_interface_up_down(interface)) != 0){
			// up-down status changed -- must update routing table with struct routing_info info
			if(up_down < 0){
				info->entries[0].cost = 16;
			}
			else{
				info->entries[0].cost = 0;
			}
			info->entries[0].address = link_interface_get_remote_virt_ip(interface);
			next_hop_addr = link_interface_get_local_virt_ip(interface);
			update_routing_table(ip_node->routing_table, ip_node->forwarding_table, info, next_hop_addr);
		}
	}
	free(info);
}
/*
struct routing_info{
	uint16_t command;
	uint16_t num_entries;
	struct cost_address entries[];
};
struct cost_address{
	uint32_t cost;
	uint32_t address;
};
void update_routing_table(routing_table_t rt, forwarding_table_t ft, struct routing_info* info, uint32_t next_hop_addr);
*/

/* takes in a uint32_t and says whether it's a local_ip or not. 
   returns:
		0 if not
		1 if it is */
static int _is_local_ip(ip_node_t ip_node, uint32_t vip){
	interface_ip_keyed_t address_keyed;

	HASH_FIND_INT(ip_node->addressToInterface, &vip, address_keyed);
	if(address_keyed)
		return 1;
	else
		return 0;
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
/* _handle_user_command_down is a helper to _handle_user_command for handling 'down <interface>' command */
static void _handle_user_command_down(ip_node_t ip_node, char* buffer){
	char* tmp = strtok(buffer, " ");
	if(!strcmp(tmp, "down")){
		tmp = strtok(NULL, " \0");
		int interface_id = atoi(tmp);
		if(((interface_id == 0)&&(strcmp(tmp, "0")))||(interface_id<0)||(interface_id >= ip_node->num_interfaces)){
			puts("Proper command: 'down <integer>' where integer corresponds to interface id printed from command 'interfaces'");
		}
		else{
			link_interface_t interface = ip_node->interfaces[interface_id];
			link_interface_bringdown(interface);
		}	
	}
}
/* _handle_user_command_up is a helper to _handle_user_command for handling 'up <interface>' command */
static void _handle_user_command_up(ip_node_t ip_node, char* buffer){
	char* tmp = strtok(buffer, " ");
	if(!strcmp(tmp, "down")){
		tmp = strtok(NULL, " \0");
		int interface_id = atoi(tmp);
		if(((interface_id == 0)&&(strcmp(tmp, "0")))||(interface_id<0)||(interface_id >= ip_node->num_interfaces)){
			puts("Proper command: 'down <integer>' where integer corresponds to interface id printed from command 'interfaces'");
		}
		else{
			link_interface_t interface = ip_node->interfaces[interface_id];
			link_interface_bringup(interface);
		}	
	}
}
/* _handle_user_command_send is a helper to _handle_user_command for handling 'send vip proto string' command */
static void _handle_user_command_send(ip_node_t ip_node, char* buffer){
	char* tmp = strtok(buffer, " ");
	if(!strcmp(tmp, "send")){
		// user using command: 'send vip proto string'
		struct in_addr send_to;
		uint32_t send_to_vip;
			
		tmp = strtok(NULL, " ");
		char* send_to_vip_string = tmp;
			
		if(inet_pton(AF_INET, send_to_vip_string, &send_to) < 0){
			puts("Proper command: 'send vip proto string' where vip is the destination virtual ip address in dotted quad notation.");
			return;
		}
		send_to_vip = send_to.s_addr;
			
		tmp = strtok(NULL, " ");
		int proto = atoi(tmp);
		if((proto == 0)&&(strcmp(tmp, "0"))){
			puts("Proper command: 'send vip proto string' where proto is an integer");
			return;
		}
		//char message[UDP_PACKET_MAX_SIZE-25];
		tmp = strtok(NULL, " \0");
		//message = tmp;
		// get next hop for sending message to send_to_vip
		uint32_t next_hop_addr = forwarding_table_get_next_hop(ip_node->forwarding_table, send_to_vip);
		if(next_hop_addr < 0){
			printf("Cannot reach address %s.\n", send_to_vip_string);
			return;
		}
		interface_ip_keyed_t address_keyed;
		HASH_FIND_INT(ip_node->addressToInterface, &next_hop_addr, address_keyed);
		if(!address_keyed){
			puts("ERROR: forwarding_table local_vip as next hop with no corresponding interface entry in hashmap addressToInterface");
			return;
		}
		link_interface_t next_hop_interface = address_keyed->interface;
		
		// wrap message in IP packet
		char to_send[UDP_PACKET_MAX_SIZE];
		
				
		// send IP packet
				
	}
}
/* _handle_user_command does exactly what it says. Note: this is only called 
   if reading from STDIN won't block, so just do it already. 

	RECOGNIZED COMMANDS:
		- interfaces : print out all interfaces
		- routes     : print out routes to known destination
		- quit/q     : stop everything

*/
static void _handle_user_command(ip_node_t ip_node){
	char* buffer = (char*) malloc(sizeof(char)*BUFFER_SIZE);
	fgets(buffer, BUFFER_SIZE-1, stdin);
	rtrim(buffer, "\n");
	
	//// handle the commands
	if(!strcmp(buffer, "quit") || !strcmp(buffer, "q"))
		ip_node->running = 0;
	
	else if(!strcmp(buffer, "interfaces"))
		ip_node_print_interfaces(ip_node);

	else if(!strcmp(buffer, "routes"))
		routing_table_print(ip_node->routing_table);
		
	else if(buffer[0] == 'd')
		_handle_user_command_down(ip_node, buffer);
	
	else if(buffer[0] == 'u')
		_handle_user_command_up(ip_node, buffer);
	
	else if(buffer[0] == 's')
		_handle_user_command_send(ip_node, buffer);

	else
		printf("Received unrecognized input from user: %s\n", buffer); 
	
	free(buffer); 
}
/* Helper to _handle_selected to clean up code -- just does the error printing */
static void _handle_selected_printerror(int error, char* buffer){
	if(error == INTERFACE_ERROR_WRONG_ADDRESS){
		puts("Received a message from incorrect address. Discarding."); 
	}
	else if(error == INTERFACE_ERROR_FATAL){
		puts("Fatal error in the interface."); 
	}
	else{
		printf("Got as result of checking validity: %d\n", ip_check_valid_packet(buffer, error));
	}
}
/* Helper to _handle_selected:
	Takes a packet that arrived but needs to be forwarded.  Handles the forwarding.
*/
static void _handle_selected_forward(ip_node_t ip_node, uint32_t dest_addr, char* packet_buffer, int bytes_read){
	uint32_t next_hop = forwarding_table_get_next_hop(ip_node->forwarding_table, dest_addr);
	
	interface_ip_keyed_t address_keyed;
	HASH_FIND_INT(ip_node->addressToInterface, &next_hop, address_keyed);
	if(!address_keyed){
		puts("ERROR: forwarding_table local_vip as next hop with no corresponding interface entry in hashmap addressToInterface");
		return;
	}
	link_interface_t next_hop_interface = address_keyed->interface;
	link_interface_send_packet(next_hop_interface, packet_buffer, bytes_read);
}
/* _handle_selected_RIP is a helper to _handle_selected:
	
*/
static void _handle_selected_RIP(ip_node_t ip_node, link_interface_t interface, char* packet_unwrapped){
	// cast packet data as routing_info
	struct routing_info* info = (struct routing_info*) packet_unwrapped; 
	
	if((ntohs(info->command) == RIP_COMMAND_REQUEST)&&(ntohs(info->num_entries) == 0)){
		//TODO: handle request 
		
	}
	else if(ntohs(info->command) == RIP_COMMAND_RESPONSE){
		update_routing_table(ip_node->routing_table, 
			ip_node->forwarding_table, info, link_interface_get_local_virt_ip(interface));
	}	
	else{
		puts("Bad RIP packet -- discarding packet");
	}
}
/* _handle_selected is a dummy function for testing the functionality of the rest
   of the system (and not the implementation of the link_interface). This will be 
   done by linking to a dummy link_interface file that provides the same methods */
static void _handle_selected(ip_node_t ip_node, link_interface_t interface){
	char* packet_buffer = (char*)malloc(sizeof(char)*IP_PACKET_MAX_SIZE + 1);
	int bytes_read = link_interface_read_packet(interface, packet_buffer, IP_PACKET_MAX_SIZE);
	if(bytes_read < 0){
		//Error -- discard packet
		_handle_selected_printerror(bytes_read, packet_buffer);
		//// clean up
		free(packet_buffer);
		return;
	}
	int packet_data_size = 	ip_check_valid_packet(packet_buffer, bytes_read);	
 	if(packet_data_size < 0){
 		puts("Discarding packet");
 		free(packet_buffer);
		return;
	}
	uint32_t dest_addr = ip_get_dest_addr(packet_buffer);
	if(!(_is_local_ip(ip_node, dest_addr))){ 
		// Must forward packet to destination -- first decrement TTL
		if(ip_decrement_TTL(packet_buffer) > 0){ // if -1 returned, must drop packet instead of forwarding
			// Time-to-live > 0: Forward packet to destination:
			_handle_selected_forward(ip_node, dest_addr, packet_buffer, bytes_read);
		}	
		free(packet_buffer);
		return;
	}
	// else either RIP data or TEST_DATA to print:
	char packet_unwrapped[packet_data_size];
	int type = ip_unwrap_packet(packet_buffer, packet_unwrapped, packet_data_size);
	if(type == RIP_DATA){
		_handle_selected_RIP(ip_node, interface, packet_unwrapped);
		struct routing_info* info = (struct routing_info*) packet_unwrapped; 
		update_routing_table(ip_node->routing_table, 
		ip_node->forwarding_table, info, link_interface_get_local_virt_ip(interface));
	}
	else if (type == TEST_DATA){
		printf("Message Received: %s\n", packet_unwrapped);
	}
	else{
		puts("Error -- packet type neither RIP nor TEST_DATA: discarding packet");
	}
	//// clean up
	free(packet_buffer);
}

