#include <inttypes.h>
#include <netinet/ip.h>
#include <time.h>


#include "bqueue.h"
#include "forwarding_table.h"
#include "routing_table.h"
#include "link_interface.h"
#include "parselinks.h"
#include "utils.h"
#include "ip_node.h"

//// select
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

//// hash-map 
#include "uthash.h"

// helpful global variables moved to ip_node.h

/*for ip packet*/
#define TCP_DATA 6
#define RIP_DATA 200  
#define TEST_DATA 0   
#define RIP_COMMAND_REQUEST 1
#define RIP_COMMAND_RESPONSE 2
#define UPDATE_INTERFACES_HZ 5

/* Static functions for internal use */
static void _update_select_list(ip_node_t node);
static void _update_all_interfaces(ip_node_t node);
static void _handle_selected(ip_node_t ip_node, link_interface_t interface, bqueue_t *to_read);
static void _handle_reading_stdin(ip_node_t ip_node, bqueue_t *stdin_commands);
static void _handle_reading_sockets(ip_node_t ip_node, bqueue_t *to_read);
static void _handle_user_command(ip_node_t ip_node, bqueue_t *stdin_commands);
static int _is_local_ip(ip_node_t ip_node, uint32_t ip);
static void _handle_query_interfaces(ip_node_t ip_node);
static void _handle_user_command_down(ip_node_t ip_node, char* buffer);
static void _handle_to_send_queue(ip_node_t ip_node, bqueue_t *to_send);
static void _request_RIP(ip_node_t ip_node);

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

ip_node_t ip_node_init(iplist_t* links){
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
			puts("link_interface didn't init properly");
			
			// get out while you can
			ip_node->num_interfaces--;
			ip_node_destroy(&ip_node);
			free_links(links);
			return NULL;
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

void ip_node_stop(ip_node_t ip_node){	
	ip_node->running = 0;
}

void ip_node_destroy(ip_node_t* ip_node){
	//// destroy forwarding/routing tables
	forwarding_table_destroy(&((*ip_node)->forwarding_table));
	routing_table_destroy(&((*ip_node)->routing_table));
	
	//// iterate through the hash maps and destroy all of the keys/values,
	//// this will NOT destroy the interfaces
	interface_socket_keyed_t socket_keyed, tmp_sock_keyed;
	HASH_ITER(hh, (*ip_node)->socketToInterface, socket_keyed, tmp_sock_keyed){
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
/* ****************************** IP_THREADS ************************************ */
/* The following 3 function pointers run the threads of ip_node that tcp_node communicates with via queues.
	alex created ip_thread_data struct to pass in following arguments to start:
	struct ip_thread_data {
		ip_node_t ip_node;
		bqueue_t *to_send;
		bqueue_t *to_read;
		bqueue_t *stdin_commands;   // way for tcp_node to pass user input commands to ip_node
	};
*/
/* Thread to handle user commands that need to be carried out by ip_node -- dequeues commands off the stdin_commands queue*/
void *ip_command_thread_run(void *ipdata){

	ip_thread_data_t ip_data = (ip_thread_data_t)ipdata;
	ip_node_t ip_node = ip_data->ip_node;
	bqueue_t *stdin_commands = ip_data->stdin_commands;
		
	free(ip_data);
	
	// create timespec for timeout on pthread_cond_timedwait(&to_send);
	struct timespec wait_cond = {PTHREAD_COND_TIMEOUT_SEC, PTHREAD_COND_TIMEOUT_NSEC}; //
	int wait_cond_ret;
	
	while(ip_node->running){
		
		if(!bqueue_empty(stdin_commands)){
			_handle_reading_stdin(ip_node, stdin_commands); 	
		}
		else{
			// wait a moment for queue to fill -- or continue through while loop after a moment passes
			pthread_mutex_lock(&(stdin_commands->q_mtx));
        	if((wait_cond_ret = pthread_cond_timedwait(&(stdin_commands->q_cond), &(stdin_commands->q_mtx), &wait_cond))!=0){
        		if(wait_cond_ret == ETIMEDOUT){
        			// timed out
      				//puts("pthread_cond_timed_wait for to_read timed out");
      			}
      			else{
      				printf("ERROR: pthread_cond_timed_wait errored out\n");
      			}
      			// unlock and continue
      			pthread_mutex_unlock(&(stdin_commands->q_mtx));
      			continue;
      		}
			pthread_mutex_unlock(&(stdin_commands->q_mtx));
			_handle_reading_stdin(ip_node, stdin_commands); 	
		}
	}	
	pthread_exit(NULL);
}

/* Thread to handle sending tcp packets from tcp_node by dequeuing packets off the to_send queue*/
void *ip_send_thread_run(void *ipdata){

	ip_thread_data_t ip_data = (ip_thread_data_t)ipdata;
	// only need to extract ip_node and to_send
	ip_node_t ip_node = ip_data->ip_node;
	bqueue_t *to_send = ip_data->to_send;
	
	free(ip_data);
		
	// create timespec for timeout on pthread_cond_timedwait(&to_send);
	struct timespec wait_cond = {PTHREAD_COND_TIMEOUT_SEC, PTHREAD_COND_TIMEOUT_NSEC}; //
	int wait_cond_ret;
	while(ip_node->running){
		
		if(!bqueue_empty(to_send))
			_handle_to_send_queue(ip_node, to_send);		
		
		else{
			// wait a moment for queue to fill -- or continue through while loop after a moment passes
			pthread_mutex_lock(&(to_send->q_mtx));
        	if((wait_cond_ret = pthread_cond_timedwait(&(to_send->q_cond), &(to_send->q_mtx), &wait_cond))!=0){
        		if(wait_cond_ret == ETIMEDOUT){
        			// timed out
      				//puts("pthread_cond_timed_wait for to_read timed out");
      			}
      			else{
      				printf("ERROR: pthread_cond_timed_wait errored out\n");
      			}
      			// unlock and continue
      			pthread_mutex_unlock(&(to_send->q_mtx));
      			continue;
      		}
			pthread_mutex_unlock(&(to_send->q_mtx));
			_handle_to_send_queue(ip_node, to_send);				
		}
	}
	pthread_exit(NULL);
}

/* ip_node_start will take just the ip_node as a parameter and will start
   up the whole process of listening to all the interfaces, 
   pushes packets for tcp to handle into the to_read queue */
void *ip_link_interface_thread_run(void *ipdata){

	ip_thread_data_t ip_data = (ip_thread_data_t)ipdata;
	// only need to extract ip_node and to_read for this thread
	ip_node_t ip_node = ip_data->ip_node;
	bqueue_t *to_read = ip_data->to_read;	//--- tcp data that ip pushes on to queue for tcp to handle
	
	free(ip_data);
	
	int retval;

	//// init the timeval struct for breaking out of select
	struct timeval tv;
	tv.tv_sec = SELECT_TIMEOUT;
	tv.tv_usec = 0;	
	// do this to init the forwarding tables/routing tables
	_handle_query_interfaces(ip_node);

	// send out RIP request message on all interfaces
	_request_RIP(ip_node);

	time_t last_update,now;
	time(&last_update);
	while(ip_node->running){
		puts("still running.");

		//// first update the list (rebuild it)
		_update_select_list(ip_node);

		//// make sure you didn't error out, otherwise pass off to _handle_reading_sockets
		retval = select(ip_node->highsock + 1, &(ip_node->read_fds), NULL, NULL, &tv);
		routing_table_check_timers(ip_node->routing_table, ip_node->forwarding_table);
		if (retval == -1)
			{ error("select()"); } 
		else if (retval){	  
			_handle_query_interfaces(ip_node); // in case the user shut down a node
			_handle_reading_sockets(ip_node, to_read);
		}	 
		else{
			_handle_query_interfaces(ip_node); 
		}
	
		time(&now);
		if(difftime(now, last_update) > UPDATE_INTERFACES_HZ){
			_update_all_interfaces(ip_node);
			time(&last_update);
		}
	}
	puts("interafce thread done");
	//pthread_exit(NULL);
	return NULL;
}
/* ************************ END OF IP_THREADS ******************************* */

/*************************** INTERNAL ******************************/
/* just reads from off each command from stdin_commands queue and handles until queue empty */
static void _handle_reading_stdin(ip_node_t ip_node, bqueue_t *stdin_commands){
	_handle_user_command(ip_node, stdin_commands);
}

/* _handle_reading_sockets is an internal function for dealing with the 
   effect of a select call. This will get called only if there is a fd to
   read from. First check STDIN, and handle that command. Then check all 
   of the interface sockets by iterating over the hashmap of sockets to interfaces. */
static void _handle_reading_sockets(ip_node_t ip_node, bqueue_t *to_read){
	struct interface_socket_keyed *socket_keyed, *tmp;
	HASH_ITER(hh, ip_node->socketToInterface, socket_keyed, tmp){
		if(FD_ISSET(socket_keyed->socket, &(ip_node->read_fds))){
			_handle_selected(ip_node, socket_keyed->interface, to_read);	
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
	struct routing_info *info = (struct routing_info *)malloc(sizeof(struct routing_info) + sizeof(struct cost_address)*2);

	// command and num_entries will be the same for each interface
	info->command = htons(2);
	info->num_entries = htons(1);

	// set up variables to iteration
	uint32_t next_hop_addr;
	struct interface_socket_keyed *socket_keyed, *tmp;

	// iterate through interfaces in hashmap
	HASH_ITER(hh, ip_node->socketToInterface, socket_keyed, tmp){
		link_interface_t interface = socket_keyed->interface;
		// check if interface status on whether up or down has changed since last checked
		int up_down;
		// Note: link_interface_query_up_down different than link_interface_up_down: the former keeps track of whether routing table must be updated
		if((up_down = link_interface_query_up_down(interface)) != 0){
			// up-down status changed -- must update routing table with struct routing_info info
			if(up_down < 0){
				routing_table_bring_down(ip_node->routing_table, ip_node->forwarding_table, link_interface_get_local_virt_ip(interface));
			}
			else{
				info->entries[0].cost = htons(0); 
	
				info->entries[0].address = link_interface_get_local_virt_ip(interface);
				next_hop_addr = link_interface_get_local_virt_ip(interface);

				update_routing_table(ip_node->routing_table, ip_node->forwarding_table, info, next_hop_addr, INTERNAL_INFORMATION);
			}
		}
	}
	free(info);
}

static void _request_RIP(ip_node_t ip_node){
	struct routing_info* request = (struct routing_info *)malloc(sizeof(struct routing_info));
	
	request->command = htons(RIP_COMMAND_REQUEST);
	request->num_entries = htons((uint16_t)0);
	
	// iterate through interfaces in hashmap
	struct interface_socket_keyed *socket_keyed, *tmp;
	HASH_ITER(hh, ip_node->socketToInterface, socket_keyed, tmp){
		link_interface_t interface = socket_keyed->interface;
		//send out RIP request on interface
		ip_wrap_send_packet_RIP(request, sizeof(struct routing_info), interface);
	}	
	free(request);
}

/* Iterates through interfaces:  For each interface that is up -- sends out RIP_RESPONSE on interface */
static void _update_all_interfaces(ip_node_t ip_node){
	int size;
	uint32_t ip;
	struct routing_info* route_info;
	interface_ip_keyed_t ip_keyed, tmp;
	link_interface_t interface;

	//// iterate through the ip_keyed interfaces (need to pull out their ip, so why not?)
	HASH_ITER(hh, ip_node->addressToInterface, ip_keyed, tmp){
		interface = ip_keyed->interface;
		ip = ip_keyed->ip;		

		/* get routing_info */
		route_info = routing_table_RIP_response(ip_node->routing_table, ip, &size, EXTERNAL_INFORMATION);
		
		/* if it's not null (nothing to send) and the interface is up, then send it out.
		   although I think we should leave the check of up/down to the interface itself.
		   ie we don't actually care whether it gets sent here, so lets just try to send it
           and if the interface is down it just won't get it out. */
		if(route_info){
			ip_wrap_send_packet_RIP((char*)route_info, size, interface);
		}	
		free(route_info);
	}
}

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
   is up before adding it  */ 

static void _update_select_list(ip_node_t ip_node){
	FD_ZERO(&(ip_node->read_fds));
	/*FD_SET(STDIN, &(ip_node->read_fds));  stdin read by tcp_node */
	int max_fd = 0;	
	int sfd;		
	
	int i;
	for(i=0;i<ip_node->num_interfaces;i++){
		if(link_interface_up_down(ip_node->interfaces[i]) > 0){
			sfd = link_interface_get_sfd(ip_node->interfaces[i]);
			max_fd = (sfd > max_fd ? sfd : max_fd);
			FD_SET(sfd, &(ip_node->read_fds));
		}
	}
	ip_node->highsock = max_fd;
}
/* _handle_user_command_down is a helper to _handle_user_command for handling 'down <interface>' command */
static void _handle_user_command_down(ip_node_t ip_node, char* buffer){
	char down[50];
	int interface_id;

	if((sscanf(buffer, "%s %d", down, &interface_id)) != 2){
		puts("Proper command: 'down <integer>' where integer corresponds to interface id printed from command 'interfaces'");
		return;
	}		

	if((interface_id<0)||(interface_id >= ip_node->num_interfaces)){
		puts("Proper command: 'down <integer>' where integer corresponds to interface id printed from command 'interfaces'");
	}
	else{
		link_interface_t interface = ip_node->interfaces[interface_id];
		link_interface_bringdown(interface);
	}		
}
/* _handle_user_command_up is a helper to _handle_user_command for handling 'up <interface>' command */
static void _handle_user_command_up(ip_node_t ip_node, char* buffer){
	char up[50];
	int interface_id;

	if((sscanf(buffer, "%s %d", up, &interface_id)) != 2){
		puts("Proper command: 'up <integer>' where integer corresponds to interface id printed from command 'interfaces'");
		return;
	}		

	if((interface_id<0)||(interface_id >= ip_node->num_interfaces)){
		puts("Proper command: 'up <integer>' where integer corresponds to interface id printed from command 'interfaces'");
	}
	else{
		link_interface_t interface = ip_node->interfaces[interface_id];
		link_interface_bringup(interface);
	}		
}
/* _handle_user_command_send iterates through to_send queue to handle each packet that has been wrapped by tcp_node */
static void _handle_to_send_queue(ip_node_t ip_node, bqueue_t *to_send){
	puts("in ip_node: _handle-to_send_queue");
	tcp_packet_data_t *tcp_packet_data = (tcp_packet_data_t *) malloc(sizeof(struct tcp_packet_data));
	char* packet = (char*) malloc(sizeof(char)*MTU);
	struct in_addr send_to, send_from;
	uint32_t send_to_vip;
	int packet_size;
	
	while(!(bqueue_trydequeue(to_send, (void **)&tcp_packet_data))){
		// extract packet data from tcp_packet_data
		packet = tcp_packet_data->packet;
		send_to_vip = tcp_packet_data->remote_virt_ip;
		packet_size = tcp_packet_data->packet_size;
		
		send_to.s_addr = send_to_vip;
	
		// check if send_to_vip local -- if so must just print
		if(_is_local_ip(ip_node, send_to_vip)){
			printf("We send a message to ourselves?  Look into how we should handle packet: %s\n", packet);
			continue;
		}
		
		// get next hop for sending message to send_to_vip
		uint32_t next_hop_addr = forwarding_table_get_next_hop(ip_node->forwarding_table, send_to_vip);
		if(next_hop_addr == -1){
			printf("Cannot reach address %d.\n", send_to_vip);
			continue;
		}
		// get struct in_addr corresponding to next_hop_addr
		send_from.s_addr = next_hop_addr;
		// get interface to send out packet on -- interface corresponding to next_hop_addr
		interface_ip_keyed_t address_keyed;
		HASH_FIND(hh, ip_node->addressToInterface, &next_hop_addr, sizeof(uint32_t), address_keyed);
		if(!address_keyed){
			printf("Cannot reach address %d  -- TODO: MAKE SURE FIXED -- see _handle_user_command_send\n", send_to_vip);
			continue;
		}
		link_interface_t next_hop_interface = address_keyed->interface;
				
		// wrap and send IP packet
		ip_wrap_send_packet(packet, packet_size, TCP_DATA, send_from, send_to, next_hop_interface);		
	}	
	free(packet);
	free(tcp_packet_data);
}

/* 
	RECOGNIZED COMMANDS:
		- interfaces : print out all interfaces
		- routes     : print out routes to known destination
		- quit/q     : stop everything

*/
static void _handle_user_command(ip_node_t ip_node, bqueue_t *stdin_commands){

	char *buffer;

/* bqueue_trydequeue attempts to dequeue an item from the queue... if there are no items
 * in the queue, rather than blocking we simply return 1 and *data has
 * an undefined value */
	while(!(bqueue_trydequeue(stdin_commands, (void **)&buffer))){
	
		rtrim(buffer, "\n");
		
		//// handle the commands
		if(!strcmp(buffer, "quit") || !strcmp(buffer, "q")){
			ip_node_stop(ip_node);
		}
		else if(!strcmp(buffer, "interfaces"))
			ip_node_print_interfaces(ip_node);
	
		else if(!strcmp(buffer, "routes"))
			routing_table_print(ip_node->routing_table);
			
		else if(buffer[0] == 'd')
			_handle_user_command_down(ip_node, buffer);
		
		else if(buffer[0] == 'u')
			_handle_user_command_up(ip_node, buffer);
	
		else if(!strcmp(buffer, "fp"))
			forwarding_table_print(ip_node->forwarding_table);	
		
		else if(!strcmp(buffer, "rp"))
			routing_table_print(ip_node->routing_table);

		else if(!strcmp(buffer, "print"))
			ip_node_print(ip_node);
	
		else
			printf("Received unrecognized input from user: %s\n", buffer); 	
	}
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
	else if(error == INTERFACE_DOWN){
		return;
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
		puts("Cannot forward packet to destination -- cannot reach destination");
		return;
	}
	link_interface_t next_hop_interface = address_keyed->interface;
	link_interface_send_packet(next_hop_interface, packet_buffer, bytes_read);
}

static void _handle_selected_RIP(ip_node_t ip_node, link_interface_t interface, char* packet_unwrapped){
	// cast packet data as routing_info
	struct routing_info* info = (struct routing_info*) packet_unwrapped; 
	
	if((ntohs(info->command) == RIP_COMMAND_REQUEST) && !ntohs(info->num_entries)){
		struct routing_info* route_info;
		int size;
		uint32_t address;

		address = link_interface_get_local_virt_ip(interface);
		
		//// get the info from the routing_table, but not the normal info
		//// that you send out every 5 seconds. Because you just received a request,
		//// probably from your neihbor, you need to tell them the truth. If you 
		//// didn't, for instance, and you sent them a poisoned-reverse response,
		//// then how would they ever know that you're their neighbor?
		route_info = routing_table_RIP_response(ip_node->routing_table,address,&size,INTERNAL_INFORMATION);
		if(route_info){
			ip_wrap_send_packet_RIP((char*)route_info, size, interface);
			free(route_info);
		}	
	}
	else if(ntohs(info->command) == RIP_COMMAND_RESPONSE){
		update_routing_table(ip_node->routing_table, 
			ip_node->forwarding_table, info, link_interface_get_local_virt_ip(interface), EXTERNAL_INFORMATION);
	}	
	else{
		printf("Bad RIP packet: command=%d\n", ntohs(info->command));
	}
 }


/* _handle_selected is a dummy function for testing the functionality of the rest
   of the system (and not the implementation of the link_interface). This will be 
   done by linking to a dummy link_interface file that provides the same methods */
static void _handle_selected(ip_node_t ip_node, link_interface_t interface, bqueue_t *to_read){
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
	char packet_unwrapped[packet_data_size+1];
	int type = ip_unwrap_packet(packet_buffer, packet_unwrapped, packet_data_size);
	
	if(type == RIP_DATA){
		_handle_selected_RIP(ip_node, interface, packet_unwrapped);
	}
	else if(type == TCP_DATA){
		//HANDLE WITH TCP
			/* enqueue an item into the queue. if the queue has been
			 * destroyed, returns -EINVAL */
		if(bqueue_enqueue(to_read, packet_unwrapped) == -EINVAL)
			puts("Tried to enqueue item into to_read queue after queue destroyed: see ip_node: _handle_selected()");
		
		packet_unwrapped[packet_data_size] = '\0'; //null terminate string so that it prints nicely
		printf("TCP Message Received: %s\n", packet_unwrapped);		
	}
	else if (type == TEST_DATA){
		packet_unwrapped[packet_data_size] = '\0'; //null terminate string so that it prints nicely
		printf("Message Received: %s\n", packet_unwrapped);
	}
	else{
		packet_unwrapped[packet_data_size] = '\0'; // null terminate string so that it prints nicely
		printf("Received packet of type neither RIP nor TEST_DATA: %s\n", packet_unwrapped);
	}
	//// clean up
	free(packet_buffer);
}

void ip_node_print(ip_node_t ip_node){
	char* ip_buffer = malloc(sizeof(char)*INET_ADDRSTRLEN);

	interface_ip_keyed_t ip_keyed, tmp;
	HASH_ITER(hh, ip_node->addressToInterface, ip_keyed, tmp){
		inet_ntop(AF_INET, (void*)&ip_keyed->ip, ip_buffer, INET_ADDRSTRLEN);
		printf("%s\n", ip_buffer);
	}
}

// returns 1 if true, 0 if false
int ip_node_running(ip_node_t ip_node){
	return ip_node->running;
}


