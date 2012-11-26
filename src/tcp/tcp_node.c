// stuct tcp_node owns a table described in WORKING_ON
#include <inttypes.h>
#include <netinet/ip.h>
#include <time.h>
#include <sys/time.h>

#include "tcp_node_stdin.h"
#include "util/utils.h"
#include "util/list.h"
#include "util/parselinks.h"

//#include "bqueue.h" -- defined in header ip_node.h
#include "ip_node.h"
#include "tcp_node.h"

//// select
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

//// hash-map 
#include "uthash.h"

// port_tuple = remote_port -- local_port
#define _port_tuple_init(to_set, remote_port, local_port)	\
do{															\
	*to_set = 0;											\
	*to_set += local_port;									\
	*to_set += ( remote_port << 16 );						\
}															\
while(0);


// static functions
static void _handle_packet(tcp_node_t tcp_node, tcp_packet_data_t tcp_packet);
//static void _handle_read(tcp_node_t tcp_node);
static int _start_ip_threads(tcp_node_t tcp_node, 
			pthread_t* ip_link_interface_thread, pthread_t* ip_send_thread, pthread_t* ip_command_thread);
static int _start_stdin_thread(tcp_node_t tcp_node, pthread_t* tcp_stdin_thread);
// inserts connection in array of connections -- grows the array if necessary
// returns number of connections in array
static int _insert_connection_array(tcp_node_t tcp_node, tcp_connection_t connection);


/*********************** Hash Table Maintenance ****************************/

/* uthash works by keying on a field of a struct, and using that key as 
   the hashmap key, therefore, it would be impossible to create two hashmaps
   just using the link_interface struct without duplicating each of the 
   structs. The solution is to create structs that extract the keyed field. 
   We have two of these in order to have hashmaps that map the socket, and the ip, 
   and they are named accordingly */

struct connection_virt_socket_keyed{
	tcp_connection_t connection;
	int virt_socket;

	UT_hash_handle hh;
};

struct connection_port_keyed{
	tcp_connection_t connection;
	uint16_t port;

	UT_hash_handle hh;
};

typedef struct connection_virt_socket_keyed* connection_virt_socket_keyed_t;
typedef struct connection_port_keyed* connection_port_keyed_t;

/* CTORS/DTORS */

/* These are straightforward in the case of the keyed structs. Just store the connection
   and pull out the socket/port */

connection_virt_socket_keyed_t connection_virt_socket_keyed_init(tcp_connection_t tcp_connection){
	connection_virt_socket_keyed_t virt_sock_keyed = malloc(sizeof(struct connection_virt_socket_keyed));
	virt_sock_keyed->virt_socket = tcp_connection_get_socket(tcp_connection);
	virt_sock_keyed->connection = tcp_connection;
	return virt_sock_keyed;
}

//// DO NOT DESTROY THE CONNECTION
void connection_virt_socket_keyed_destroy(connection_virt_socket_keyed_t* sock_keyed){
	free(*sock_keyed);
	*sock_keyed = NULL;
}

connection_port_keyed_t connection_port_keyed_init(tcp_connection_t connection){
	connection_port_keyed_t port_keyed = malloc(sizeof(struct connection_port_keyed));
	_port_tuple_init(port_keyed->port, tcp_connection_get_local_port(connection), tcp_connection_get_remote_port(connection));
	port_keyed->connection = connection;
	
	return port_keyed;
}

//// DO NOT DESTROY THE CONNECTION
void connection_port_keyed_destroy(connection_port_keyed_t* port_keyed){
	free(*port_keyed);
	*port_keyed = NULL;
}
/***************** End of Hash Table Maintenance ************************/


struct tcp_node{
	int running; // 0 for not running, 1 for running // mimics ip_node
	ip_node_t ip_node;	// owns an ip_node_t that it keeps running to send/receive/listen on its interfaces
	
	// owns table of tcp_connections
		// each tcp_connection owns a statemachine to keep track of its state

	/****** Kernal Related *********/
	 //This mtx should be locked any time we are accessing/changing kernal
	 // where kernal is any of the data structures/ints in this 'kernal related' section
	pthread_mutex_t kernal_mutex;
	
	int num_connections; //number of current tcp_connections
	int connection_array_size; // = MAX_FILE_DESCRIPTORS;
	
	tcp_connection_t* connections;
	connection_virt_socket_keyed_t virt_socketToConnection;
	connection_port_keyed_t portToConnection;	
	// used to systematically keep track of available file descriptors/ports and to reuse them after socket closed
	// each time need new unique socket/port, call dequeue and item is pointer to int to use as socket/port
	int_queue_t sockets_available_queue;
	int_queue_t ports_available_queue;
	/****** End of Kernal Related *********/

	/******* Thread Related **************/
	bqueue_t *to_send;	//--- tcp data for ip to send
	bqueue_t *to_read;	//--- tcp data that ip pushes on to queue for tcp to handle
	bqueue_t *stdin_commands;	//---  way for tcp_node to pass user input commands to ip_node

	plain_list_t thread_list;
	/******* End of Thread Related **************/
};

tcp_node_t tcp_node_init(iplist_t* links){
	// initalize ip_node -- return FAILURE if ip_node a failure
	ip_node_t ip_node = ip_node_init(links);
	if(!ip_node)
		return NULL;
	// create tcp_node
	tcp_node_t tcp_node = (tcp_node_t)malloc(sizeof(struct tcp_node));	
	tcp_node->ip_node = ip_node;
	
	/************ Create Queues ********************/			
	// create to_send and to_read stdin_commands queues that allow tcp_node and ip_node to communicate
	// to_send is loaded with tcp packets by tcp_node that need to be sent by ip_node
	bqueue_t *to_send = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(to_send);
	tcp_node->to_send = to_send;
	// to_read is loaded with unwrapped ip_packets by ip_node that tcp_node needs to handle and unwrap as tcp packets
	bqueue_t *to_read = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(to_read);
	tcp_node->to_read = to_read;
	// stdin_commands is for the tcp_node to pass user input commands to ip_node that ip_node needs to handle
	bqueue_t *stdin_commands = (bqueue_t*) malloc(sizeof(bqueue_t));   // way for tcp_node to pass user input commands to ip_node
	bqueue_init(stdin_commands);	
	tcp_node->stdin_commands = stdin_commands;
	/************ Queues Created *****************/
	/*********** create kernal table  ************/
	pthread_mutex_init(&(tcp_node->kernal_mutex), NULL);
	tcp_node->connection_array_size = MAX_FILE_DESCRIPTORS;  
	tcp_node->num_connections = 0; // no connections at start
	tcp_node->connections = (tcp_connection_t*)malloc(sizeof(tcp_connection_t)*(tcp_node->connection_array_size));
	
	tcp_node->virt_socketToConnection = NULL;
	tcp_node->portToConnection = NULL;
	
	/* Initialize sockets/ports available queues */
	int_queue_t sockets_available_queue = int_queue_init();
	int_queue_t ports_available_queue = int_queue_init();
	
	int_queue_set_size(sockets_available_queue, MAX_FILE_DESCRIPTORS);
	int_queue_set_size(ports_available_queue, MAX_FILE_DESCRIPTORS);
	
	int i;
	for(i=0; i<MAX_FILE_DESCRIPTORS; i++){		
		/* We're just queueing the integers we want to use as sockets/ports */
		int_queue_push(sockets_available_queue, i);
		int_queue_push(ports_available_queue, (i+1)); // tcp_connection with port = 0 signifies that port hasn't been set	
	}
	
	tcp_node->sockets_available_queue = sockets_available_queue;
	tcp_node->ports_available_queue = ports_available_queue;
	/************ Kernal Table Created ***********/

	/* thread queue */
	tcp_node->thread_list = plain_list_init();
	
	//// you're still running right? right
	tcp_node->running = 1;

	return tcp_node;
}
// before shutting down, first must ABORT all connections
void tcp_node_ABORT_connections(tcp_node_t tcp_node){
	int i;
	for(i=0; i<(tcp_node->num_connections); i++){
		// we quickly send RST rather than gracefully CLOSEing
		if(tcp_node->connections[i] != NULL) //it could be NULL
			tcp_connection_ABORT(tcp_node->connections[i]);
	}
}

/* will this do it? */
void tcp_node_stop(tcp_node_t tcp_node){
	tcp_node->running = 0;
}

void tcp_node_destroy(tcp_node_t tcp_node){
	tcp_node->running = 0;
	
	/*****************************/
	plain_list_t list = tcp_node->thread_list;
	plain_list_el_t el;
	tcp_api_args_t args;
  	print(("tcp_node_destroy 0"), CLOSING_PRINT);
	PLAIN_LIST_ITER(list, el)
        args = (tcp_api_args_t)el->data;
        print(("tcp_node_destroy 0.1"), CLOSING_PRINT);
        int result = tcp_api_args_destroy(&args);
        print(("tcp_node_destroy 0.2"), CLOSING_PRINT);
		if(result < 0){	
			char* error_string = strerror(-result);
			printf("Error: %s\n", error_string);
		}
        
		else if(result==0)
			printf("successful.");
		
		else
			printf("got result: %d!\n", result);
        		
		plain_list_remove(list, el);			
	PLAIN_LIST_ITER_DONE(list);
	/*****************************/
  	print(("tcp_node_destroy 1"), CLOSING_PRINT);	
	// wait for mutex so we can ensure we destroy e'erthang
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
	//// iterate through the hash maps and destroy all of the keys/values,
	//// this will NOT destroy the connections
	connection_virt_socket_keyed_t socket_keyed, tmp_sock_keyed;
	HASH_ITER(hh, tcp_node->virt_socketToConnection, socket_keyed, tmp_sock_keyed){
		HASH_DEL(tcp_node->virt_socketToConnection, socket_keyed);
		connection_virt_socket_keyed_destroy(&socket_keyed);
	}

	//// ditto (see above)
	connection_port_keyed_t port_keyed, tmp_port_keyed;
	HASH_ITER(hh, tcp_node->portToConnection, port_keyed, tmp_port_keyed){
		HASH_DEL(tcp_node->portToConnection, port_keyed);
		connection_port_keyed_destroy(&port_keyed);
	}
	
	//// NOW destroy all the connections
	int i;
	for(i=0; i<(tcp_node->num_connections); i++){
		// we already aborted it when we called quit_cmd
		if(tcp_node->connections[i] != NULL)
			tcp_connection_destroy(tcp_node->connections[i]);

	}

	// free the array itself
	free(tcp_node->connections);
	// get rid of kernal mutex
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));
	pthread_mutex_destroy(&(tcp_node->kernal_mutex));



	// destroy ip_node and queues
	ip_node_t ip_node = tcp_node->ip_node;
	ip_node_destroy(&ip_node);
	
	// destroy bqueues
	bqueue_destroy(tcp_node->to_send);
	free(tcp_node->to_send);

	bqueue_destroy(tcp_node->to_read);
	free(tcp_node->to_read);

	bqueue_destroy(tcp_node->stdin_commands);
	free(tcp_node->stdin_commands);
	
	// destroy socket/port queues -- they just hold ints so i don't think we need to destroy each item inside
	int_queue_destroy(&(tcp_node->sockets_available_queue));
	int_queue_destroy(&(tcp_node->ports_available_queue));
	
	plain_list_destroy(&(tcp_node->thread_list));
	
	free(tcp_node);
	tcp_node = NULL;
}
/************** Functions dealing with Kernal Table  *************/


/* For accept */
/* - calls on the listening_connection to dequeue its triple and node creates new connection with information
     returned int is the new socket assigned to that new connection.  
   - The connection finishes its handshake to get to established state
   - Fills addr with ip address information from dequeued triple*/
tcp_connection_t tcp_node_connection_accept(tcp_node_t tcp_node, tcp_connection_t listening_connection){
	
	// dequeue from accept_queue of listening connection to get triple of information about new connection
	/* THIS CALL IS BLOCKING  -- since the accept queue is a bqueue_t */
	accept_queue_data_t data = tcp_connection_accept_queue_dequeue(listening_connection);
	if(data == NULL)
		return NULL; // means there was an error in dequeueing -- was accept_queue destroyed?
	
	
	// create new connection which will be the accepted connection 
	// -- function will insert it into kernal array and socket hashmap
	tcp_connection_t new_connection = tcp_node_new_connection(tcp_node);
	if(new_connection == NULL){
		// reached limit MAX_FILE_DESCRIPTORS
		accept_queue_data_destroy(&data);
		return NULL; 
	}
	
	// assign values from triple to that connection (tcp_node_new_connection assigned it a unique port)
	tcp_connection_set_local_ip(new_connection, accept_queue_data_get_local_ip(data));
	tcp_connection_set_remote(new_connection, accept_queue_data_get_remote_ip(data), accept_queue_data_get_remote_port(data));
	tcp_connection_set_last_seq_received(new_connection, accept_queue_data_get_seq(data));

	// don't we need to set the local port? because it needs to receive data
	int port = tcp_node_assign_port(tcp_node, new_connection, -1); //-1 just assigns to next port
	
	//to test:
	print(("port = tcp_node_assign_port(tcp_node, new_connection, -1) = %d\n", port), PORT_PRINT);
	print(("tcp_connection_get_local_port(new_connection) = %d\n", tcp_connection_get_local_port(new_connection)), PORT_PRINT);
	
	// destroy data -- all done with it
	accept_queue_data_destroy(&data);
	
	return new_connection;
}

// creates a new tcp_connection and properly places it in kernal table -- ports initialized to unique value, ip to 0
// returns NULL if reached limit MAX_FILE_DESCRIPTORS
tcp_connection_t tcp_node_new_connection(tcp_node_t tcp_node){
	
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
	
	if(tcp_node->num_connections == tcp_node->connection_array_size){
		pthread_mutex_unlock(&(tcp_node->kernal_mutex));
		return NULL;	// reached limit MAX_FILE_DESCRIPTORS
	}
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));
	
	int socket = tcp_node_next_virt_socket(tcp_node);
	if(socket<0) // EMPTY_QUEUE --no more available sockets
		return NULL;
		
	// init new tcp_connection
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
	tcp_connection_t connection = tcp_connection_init(tcp_node, socket, tcp_node->to_send);
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));
	
	// place connection in array
	_insert_connection_array(tcp_node, connection);

	/*
	commented out because if we call bind() on a new socket, an error
	is given (correctly?) because the connection that we're trying to 
	bind already has a port (because of the below)
	
	// place connection in hashmaps
	int port = tcp_node_next_port(tcp_node);
	tcp_node_assign_port(tcp_node, connection, port);
	*/
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
	connection_virt_socket_keyed_t socket_keyed = connection_virt_socket_keyed_init(connection);
	HASH_ADD(hh, tcp_node->virt_socketToConnection, virt_socket, sizeof(int), socket_keyed);
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));

	return connection;
}
//needs to be called when close connection so that we can return port/socket to available queue for reuse
void tcp_node_return_socket_to_kernal(tcp_node_t tcp_node, int socket){
	
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
	// return socket to available queue
	int_queue_push_front(tcp_node->sockets_available_queue, socket);
	
	connection_virt_socket_keyed_t socket_keyed;
	HASH_FIND(hh, tcp_node->virt_socketToConnection, &socket, sizeof(int), socket_keyed);
	if(!socket_keyed){
		puts("Error: Alex Neil see tcp_node_close_connection -- this SHOULD be in table");
		return;
	}

	HASH_DEL(tcp_node->virt_socketToConnection, socket_keyed);
	connection_virt_socket_keyed_destroy(&socket_keyed);
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));
}

//needs to be called when close connection so that we can return port/socket to available queue for reuse
void tcp_node_return_port_to_kernal(tcp_node_t tcp_node, int port){
	
	// port of zero means port wasn't actually set for that connection -- not a valid port
	if(!port) 
		return;
		
	// return port to available queue
	if(port<=MAX_FILE_DESCRIPTORS)
		int_queue_push_front(tcp_node->ports_available_queue, port);
	
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
		
	connection_port_keyed_t port_keyed;
	HASH_FIND(hh, tcp_node->portToConnection, &port, sizeof(uint16_t), port_keyed);
	if(!port_keyed){
		puts("Error: Alex Neil see tcp_node_close_connection -- this SHOULD be in table");
		pthread_mutex_unlock(&(tcp_node->kernal_mutex));
		return;
	}

	HASH_DEL(tcp_node->portToConnection, port_keyed);
	connection_port_keyed_destroy(&port_keyed);
	
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));
}

//###TODO: FINISH LOGIC ####
//needs to be called when close connection so that we can return port/socket to available queue for reuse
// returns new number of connections in kernal
int tcp_node_close_connection(tcp_node_t tcp_node, tcp_connection_t connection){
	puts("in tcp_node_close_connection");
	//TODO: CONNECTION CLOSING LOGIC
	tcp_connection_state_machine_transition(connection, CLOSE);
	//int num_connections = tcp_node_remove_connection_kernal(tcp_node);
	//return num_connections;
	return 0;
}	
	
int tcp_node_remove_connection_kernal(tcp_node_t tcp_node, tcp_connection_t connection){
	
	if(!connection)
		return tcp_node->num_connections;
		
	// return port and socket to available queue for reuse
	int port = (int)tcp_connection_get_local_port(connection);
	int socket = tcp_connection_get_socket(connection);
	
	// remove from kernal	- these calls lock/unlock kernal
	tcp_node_return_socket_to_kernal(tcp_node, socket);
	if(tcp_connection_get_local_port(connection))
		tcp_node_return_port_to_kernal(tcp_node, port);
	
	// lock kernal
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
	tcp_connection_destroy(connection);
		
	tcp_node->connections[socket] = NULL;
	tcp_node->num_connections = (tcp_node->num_connections) - 1;
	
	//unlock kernal
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));
	
	return tcp_node->num_connections;
}
// returns tcp_connection corresponding to socket -- locks/unlocks kernal
tcp_connection_t tcp_node_get_connection_by_socket(tcp_node_t tcp_node, int socket){
	
	//lock kernal
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
	
	connection_virt_socket_keyed_t socket_keyed;

	HASH_FIND(hh, tcp_node->virt_socketToConnection, &socket, sizeof(int), socket_keyed);
	
	//unlock kernal
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));
	
	if(!socket_keyed)
		return NULL;
	else
		return socket_keyed->connection;
}

// returns tcp_connection corresponding to port
tcp_connection_t tcp_node_get_connection_by_port(tcp_node_t tcp_node, uint16_t remote_port, uint16_t local_port){

	uint32_t combined;
	_port_tuple_init(&combined, remote_port, local_port);

	//lock kernal
	pthread_mutex_lock(&(tcp_node->kernal_mutex));

	connection_port_keyed_t port_keyed;

	/* try to find it with the combination of remote/local */
	HASH_FIND(hh, tcp_node->portToConnection, &combined, sizeof(uint32_t), port_keyed);

	/* if unsuccessful try just the remote */
	if(!port_keyed){
		HASH_FIND(hh, tcp_node->portToConnection, &((uint32_t)local_port), sizeof(uint32_t), port_keyed);
	}
	
	//unlock kernal
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));
	
	if(!port_keyed)
		return NULL;
	else
		return port_keyed->connection;
}


// assigns port to tcp_connection and puts entry in hash table that hashes ports to tcp_connections
// returns 1 if port successfully assigned, 0 otherwise

/* just a note, I feel like 0 is traditional used to indicate success */
int tcp_node_assign_port(tcp_node_t tcp_node, tcp_connection_t connection, uint16_t local_port, uint16_t remote_port){
	
	if(local_port<=0){
		local_port = tcp_node_next_port(tcp_node);
		print(("port = tcp_node_next_port(tcp_node) = %d\n", port), PORT_PRINT);
	}
		
	if(tcp_node_port_unused(tcp_node, local_port)<0)
		return -1; // port already in use
	
	// return previous port to kernal if it was previously set
	if(tcp_connection_get_local_port(connection))
		tcp_node_return_port_to_kernal(tcp_node, tcp_connection_get_local_port(connection));
		
	// set connection's port
	uint16_t uport = (uint16_t)port;
	tcp_connection_set_local_port(connection, uport);
	
	// put port to connection in kernal
	//lock kernal
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
	
	// put port to connection in kernel
	connection_port_keyed_t port_keyed = connection_port_keyed_init(connection);
	HASH_ADD(hh, tcp_node->portToConnection, port, sizeof(uint16_t), port_keyed);
	
	//unlock kernal
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));

	return port;
}

// returns 1 if the port is available for use, 0 if already in use
int tcp_node_port_unused(tcp_node_t tcp_node, int port){	

	//lock kernal
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
	
	connection_port_keyed_t port_keyed;
	// check that port not already in hashmap
	HASH_FIND(hh, tcp_node->portToConnection, &port, sizeof(uint16_t), port_keyed);
	
	//unlock kernal
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));	
	
	if(!port_keyed)
		return 1;
	else
		return -1;
}

// returns next available, currently unused, port to bind or connect/accept a new tcp_connection with
int tcp_node_next_port(tcp_node_t tcp_node){

	//lock kernal when popping off queue of available ports
	pthread_mutex_lock(&(tcp_node->kernal_mutex));
	int next_port = int_queue_pop(tcp_node->ports_available_queue);
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));
	
	// check that next_port not already in use -- not already in hashmap	
	while((tcp_node_port_unused(tcp_node, next_port))<0){
		
		pthread_mutex_lock(&(tcp_node->kernal_mutex));
		next_port = int_queue_pop(tcp_node->ports_available_queue);
		pthread_mutex_unlock(&(tcp_node->kernal_mutex));
	}
	
	return next_port;
}

// returns next available, currently unused, virtual socket file descriptor to initiate a new tcp_connection with
int tcp_node_next_virt_socket(tcp_node_t tcp_node){

	//lock kernal
	pthread_mutex_lock(&(tcp_node->kernal_mutex));

	int next_socket = int_queue_pop(tcp_node->sockets_available_queue);
	
	//unlock kernal
	pthread_mutex_unlock(&(tcp_node->kernal_mutex));	
	
	return next_socket;
}
/**************** End of functions that deal with Kernal Table *******************/

// iterate through sockets hash map to print info about each socket
void tcp_node_print(tcp_node_t tcp_node){

	//char* buffer = malloc(sizeof(char)*INET_ADDRSTRLEN);
	tcp_connection_t connection;
	int i=0;
	connection_virt_socket_keyed_t socket_keyed, tmp;
	HASH_ITER(hh, tcp_node->virt_socketToConnection, socket_keyed, tmp){
		i++;
		connection = socket_keyed->connection;
		tcp_connection_print_sockets(connection);
	}
	if(i==0)
		puts("No Open File Descriptors");	
}

void tcp_node_start(tcp_node_t tcp_node){
	/* tcp_node runs 4 threads: 
		pthread_t tcp_stdin_thread to catch	stdin and put commands in stdin_queue for tcp_node to handle
		ip_link_interface_thread:  reads off link interfaces (current select() loop) and throws packets for tcp to handle into to_read
		ip_send_thread: calls p_thread_cond_wait(&to_send) and sends things loaded on to queue
		ip_command_thread: calls p_thread_cond_wait(&stdin_commands) and handles ip commands loaded on to queue
	*/
	pthread_t tcp_stdin_thread, ip_link_interface_thread, ip_send_thread, ip_command_thread;
	
	// start up ip_node threads
	
	if(!_start_ip_threads(tcp_node, &ip_link_interface_thread, &ip_send_thread, &ip_command_thread)){
		// failed to start thread that runs ip_node_start() -- get out and destroy
		puts("Failed to start ip_node");
		return;
	}

	// start thread for reading in standard inputs
	if(!_start_stdin_thread(tcp_node, &tcp_stdin_thread)){
		// failed to start thread that runs tcp_node_stdin_thread() -- get out and destroy
		puts("Failed to start tcp_node_stdin_thread");
		return;
	}
		
	// create timespec for timeout on pthread_cond_timedwait(&to_read);
	struct timespec wait_cond;// = {PTHREAD_COND_TIMEOUT_SEC, PTHREAD_COND_TIMEOUT_NSEC}; //

	bqueue_t* to_read = tcp_node->to_read;
	struct timeval now;
	void* packet;
	int ret;
	int i=0,mod=10;
	while((tcp_node->running)&&(tcp_node_ip_running(tcp_node))){	
		if(i++%mod==0)
			print(("tcp_node still running"), TCP_PRINT); 

		/* get the time of the day so that we are passing in to bqueue_timed_dequeue_abs
			the absolute time when we want the timeout to occur (the docs in bqueue.c say
			relative, but then the argument is called abs_ts so its unclear what they intended).
			This works for now but we might need to change it when we port over to linux */
		gettimeofday(&now, NULL);	
		wait_cond.tv_sec = now.tv_sec+PTHREAD_COND_TIMEOUT_SEC;
		wait_cond.tv_nsec = 1000*now.tv_usec+PTHREAD_COND_TIMEOUT_NSEC;

        wait_cond.tv_sec += wait_cond.tv_nsec/1000000000;
        wait_cond.tv_nsec %= 1000000000;
		/* try to get the next thing on queue */
        ret = bqueue_timed_dequeue_abs(to_read, &packet, &wait_cond);
		if (ret != 0) 
			/* should probably check at this point WHY we failed (for instance perhaps the queue
				was destroyed */
			continue;
			
		/* otherwise there's a packet waiting for you! */
		_handle_packet(tcp_node, (tcp_packet_data_t)packet);
	}
	print(("broke tcp_node handling loop"), CLOSING_PRINT);
	
	ip_node_stop(tcp_node->ip_node);

	int rc;
	print(("joining link interface thread"), CLOSING_PRINT);
	rc = pthread_join(ip_link_interface_thread, NULL);
	if (rc) {
		print(("ERROR; return code from pthread_join() is %d\n", rc), CLOSING_PRINT);
		exit(-1);
	}

	print(("joining send_thread"), CLOSING_PRINT);
	rc = pthread_join(ip_send_thread, NULL);
	if (rc) {
		print(("ERROR; return code from pthread_join() is %d\n", rc), CLOSING_PRINT);
		exit(-1);
	}

	print(("joining ip_command thread"), CLOSING_PRINT);
	rc = pthread_join(ip_command_thread, NULL);
	if (rc) {
		print(("ERROR; return code from pthread_join() is %d\n", rc), CLOSING_PRINT);
		exit(-1);
	}

	print(("stdin thread"), CLOSING_PRINT);
	rc = pthread_join(tcp_stdin_thread, NULL);
	if (rc) {
		print(("ERROR; return code from pthread_join() is %d\n", rc), CLOSING_PRINT);
		exit(-1);
	}

	print(("finished."), CLOSING_PRINT);
}

// puts command on to stdin_commands queue
// returns 1 on success, -1 on failure (failure when queue actually already destroyed)
int tcp_node_queue_ip_cmd(tcp_node_t tcp_node, char* buffered_cmd){
	
	bqueue_t *stdin_commands = tcp_node->stdin_commands;
	
	if(bqueue_enqueue(stdin_commands, (void*)buffered_cmd))
		return -1;
	
	return 1;
}

// returns tcp_node->running
int tcp_node_running(tcp_node_t tcp_node){
	return tcp_node->running;
}
// returns whether ip_node running still
int tcp_node_ip_running(tcp_node_t tcp_node){
	return ip_node_running(tcp_node->ip_node);
}

void tcp_node_command_ip(tcp_node_t tcp_node, const char* cmd){
	ip_node_command(tcp_node->ip_node, cmd);
}

/*
tcp_node_invalid_port
	handles the case where a packet is received to which there is no open 
	port. Should inform the sender?
*/
void tcp_node_invalid_port(tcp_node_t tcp_node, tcp_packet_data_t packet){
	/* anything else? */
	tcp_node_refuse_connection(tcp_node, packet);
	puts("invalid port. sent RST");
}

void tcp_node_refuse_connection(tcp_node_t tcp_node, tcp_packet_data_t packet){
/*  
	RFC 793: pg 35
	If the connection does not exist (CLOSED) then a reset is sent
    in response to any incoming segment except another reset.	****
    In particular, SYNs addressed to a non-existent connection are rejected
    by this means.
*/  
	struct tcphdr* incoming_header = (struct tcphdr*)packet->packet;

	if(tcp_rst_bit(incoming_header))
		return; //by **** a few lines right above

	// create the outgoing packet
	struct tcphdr* outgoing_header = tcp_header_init(0);

	/* PORTS */
	tcp_set_dest_port(outgoing_header, tcp_source_port(incoming_header));
	tcp_set_source_port(outgoing_header, tcp_dest_port(incoming_header));

	/* RST */
	tcp_set_rst_bit(outgoing_header);
	
	/* seqnum */
		/* If the incoming segment has an ACK field, the reset takes its
		sequence number from the ACK field of the segment, otherwise the
		reset has sequence number zero and the ACK field is set to the sum
		of the sequence number and segment length of the incoming segment.
		The connection remains in the CLOSED state. */
	if(tcp_ack_bit(incoming_header)){
		tcp_set_seq(outgoing_header, tcp_ack(incoming_header));
	}
	else{
		tcp_set_seq(outgoing_header, 0);
		/* ack */
		int seg_length = packet->packet_size - tcp_offset_in_bytes(incoming_header) + 1;
		tcp_set_ack(outgoing_header, (tcp_seqnum(incoming_header)+seg_length));
		tcp_set_ack_bit(outgoing_header);
	}

	/* CHECKSUM */
	tcp_utils_add_checksum(outgoing_header, sizeof(*outgoing_header), packet->local_virt_ip, packet->remote_virt_ip, TCP_DATA);

	tcp_packet_data_t rst_packet = tcp_packet_data_init((char*)outgoing_header, sizeof(*outgoing_header), packet->local_virt_ip, packet->remote_virt_ip);
	
	/* SEND IT OFF */
	ip_node_send_tcp(tcp_node->ip_node, rst_packet);

	
	return;
}
	

/************************** Internal Functions Below ********************************************/

// inserts connection in array of connections -- sfd id is the index
// returns number of connections in array
static int _insert_connection_array(tcp_node_t tcp_node, tcp_connection_t connection){
	
	int num_connections = tcp_node->num_connections;
	int connection_array_size = tcp_node->connection_array_size;
	
	if(num_connections+1 > connection_array_size){
		return -1; // reached max size
	}
	// insert new connection in array
	tcp_node->connections[num_connections] = connection;
	tcp_node->num_connections++;
	
	return tcp_node->num_connections;
}

static void _handle_packet(tcp_node_t tcp_node, tcp_packet_data_t tcp_packet){
		
	int packet_size = tcp_packet->packet_size;
	if( packet_size < TCP_HEADER_MIN_SIZE ){
		puts("packet received is less than header size, discarding...");
		tcp_packet_data_destroy(&tcp_packet); //<--CAN'T JUST FREE -- caused segfaults
		free(tcp_packet);
		return;
	}

	uint16_t local_port   = tcp_dest_port(tcp_packet->packet);
	uint32_t remote_port  = tcp_src_port(tcp_packet->packet);

	tcp_connection_t connection = tcp_node_get_connection_by_port(tcp_node, local_port, remote_port);
	if(!connection){
		printf("invalid port: %u\n", dest_port);
		tcp_node_invalid_port(tcp_node, tcp_packet);
		tcp_packet_data_destroy(&tcp_packet); //<--CAN'T JUST FREE -- caused segfaults
		return;
	}
	// put it on that connection's my_to_read queue
	tcp_connection_queue_to_read(connection, tcp_packet);
}



/* helper function to tcp_node_start -- does the work of starting up _handle_tcp_node_stdin() in a thread */
static int _start_stdin_thread(tcp_node_t tcp_node, pthread_t* tcp_stdin_thread){		
	
	/* Initialize and set thread detached attribute */
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		
	int status = pthread_create(tcp_stdin_thread, &attr, _handle_tcp_node_stdin, (void *)tcp_node);
    if (status){
         printf("ERROR; return code from pthread_create() for _handle_tcp_node_stdin is %d\n", status);
         return 0;
    }
    pthread_attr_destroy(&attr);
	return 1;
}

/* helper function to tcp_node_start -- does the work of starting up ip_node_start() in a thread */
static int _start_ip_threads(tcp_node_t tcp_node, 
			pthread_t* ip_link_interface_thread, pthread_t* ip_send_thread, pthread_t* ip_command_thread){
	/*struct ip_thread_data{
		ip_node_t ip_node;
		bqueue_t *to_send;
		bqueue_t *to_read;
		bqueue_t *stdin_commands;   // way for tcp_node to pass user input commands to ip_node
	};*/
	// fetch and put arguments into ip_thread_data_t  -- each thread responsible for freeing its own ip_thread_data arg
	ip_thread_data_t ip_data_link_interface_thread = (ip_thread_data_t)malloc(sizeof(struct ip_thread_data));
	ip_data_link_interface_thread->ip_node = tcp_node->ip_node;
	ip_data_link_interface_thread->to_read = tcp_node->to_read;
	
	ip_thread_data_t ip_data_send_thread = (ip_thread_data_t)malloc(sizeof(struct ip_thread_data));
	ip_data_send_thread->ip_node = tcp_node->ip_node;
	ip_data_send_thread->to_send = tcp_node->to_send;
	
	ip_thread_data_t ip_data_command_thread = (ip_thread_data_t)malloc(sizeof(struct ip_thread_data));
	ip_data_command_thread->ip_node = tcp_node->ip_node;	
	ip_data_command_thread->stdin_commands = tcp_node->stdin_commands;	
	
	/* Initialize and set thread detached attribute */
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	// start up each thread	
	int status;
	status = pthread_create(ip_link_interface_thread, &attr, ip_link_interface_thread_run, (void *)ip_data_link_interface_thread);
    if (status){
         printf("ERROR; return code from pthread_create() for ip_link_interface_thread is %d\n", status);
         free(ip_data_link_interface_thread);
         return 0;
    }
    status = pthread_create(ip_send_thread, NULL, ip_send_thread_run, (void *)ip_data_send_thread);
    if (status){
         printf("ERROR; return code from pthread_create() for ip_send_thread is %d\n", status);
         free(ip_data_send_thread);
         return 0;
    }
    status = pthread_create(ip_command_thread, NULL, ip_command_thread_run, (void *)ip_data_command_thread);
    if (status){
         printf("ERROR; return code from pthread_create() for ip_command_thread is %d\n", status);
         free(ip_data_command_thread);
         return 0;
    }
    pthread_attr_destroy(&attr);
	return 1;
}

/* for threading */
void tcp_node_thread(tcp_node_t node, void *(*start_routine)(void*), tcp_api_args_t arg){
	pthread_create(&(arg->thread),NULL,start_routine,arg);
	plain_list_append(node->thread_list, arg);
}	

plain_list_t tcp_node_thread_list(tcp_node_t node){
	return node->thread_list;
}

/*********** For use by tcp_node to reach ip_node items ****************/
// returns ip address of remote side of passed in remote ip
// returns 0 if remote ip unreachable
uint32_t tcp_node_get_local_ip(tcp_node_t tcp_node, uint32_t remote_ip){
	return tcp_ip_node_get_local_ip(tcp_node->ip_node, remote_ip);
}

/***************** FOR TESTING *********************/

uint32_t tcp_node_get_interface_remote_ip(tcp_node_t tcp_node, int interface_num){
	return ip_node_get_interface_remote_ip(tcp_node->ip_node, interface_num);
}

uint32_t tcp_node_get_interface_local_ip(tcp_node_t tcp_node, int interface_num){
	return ip_node_get_interface_local_ip(tcp_node->ip_node, interface_num);
}	





