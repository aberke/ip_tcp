// stuct tcp_node owns a table described in WORKING_ON
#include <inttypes.h>
#include <netinet/ip.h>
#include <time.h>
#include <sys/time.h>

#include "tcp_utils.h"
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


// static functions
static void _handle_packet(tcp_node_t tcp_node, tcp_packet_data_t tcp_packet);
//static void _handle_read(tcp_node_t tcp_node);
static int _start_ip_threads(tcp_node_t tcp_node, 
			pthread_t* ip_link_interface_thread, pthread_t* ip_send_thread, pthread_t* ip_command_thread);
static int _start_stdin_thread(tcp_node_t tcp_node, pthread_t* tcp_stdin_thread);
// inserts connection in array of connections -- grows the array if necessary
// returns number of connections in array
static int _insert_connection_array(tcp_node_t tcp_node, tcp_connection_t connection);

// a tcp_connection in the listen state queues this triple on its accept_queue when
// it receives a syn.  Nothing further happens until the user calls accept at which point
// this triple is dequeued and a connection is initiated with this information
// the connection should then set its state to listen and go through the LISTEN_to_SYN_RECEIVED transition
struct accept_queue_triple{
	uint32_t remote_ip;
	uint16_t remote_port;
	uint32_t last_seq_received;
};


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
	int port;

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
	port_keyed->port = tcp_connection_get_local_port(connection);
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
	bqueue_t *to_send;	//--- tcp data for ip to send
	bqueue_t *to_read;	//--- tcp data that ip pushes on to queue for tcp to handle
	bqueue_t *stdin_commands;	//---  way for tcp_node to pass user input commands to ip_node
	
	// owns table of tcp_connections
		// each tcp_connection owns a statemachine to keep track of its state

	int num_connections; //number of current tcp_connections
	int connection_array_size; // will need to realloc if don't initially create enough room in array kernal
	
	tcp_connection_t* connections;
	connection_virt_socket_keyed_t virt_socketToConnection;
	connection_port_keyed_t portToConnection;
	
	// For now have silly way of making sure we don't repeat a virt_socket or port number.  TODO: FIX!!!!!
	int virt_socket_count;	//TODO: Instead of increasing virt_socket by one at each new connection, create system of storing available socket Id's
	int port_count;	//TODO: same as directly above, but with ports
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
	tcp_node->connection_array_size = START_NUM_INTERFACES;  
	tcp_node->num_connections = 0; // no connections at start
	tcp_node->connections = (tcp_connection_t*)malloc(sizeof(tcp_connection_t)*(tcp_node->connection_array_size));
	
	tcp_node->virt_socketToConnection = NULL;
	tcp_node->portToConnection = NULL;
	
	tcp_node->virt_socket_count = 0;
	tcp_node->port_count = 0;
	/************ Kernal Table Created ***********/
	
	//// you're still running right? right
	tcp_node->running = 1;
	
	return tcp_node;
}

/* will this do it? */
void tcp_node_stop(tcp_node_t tcp_node){
	tcp_node->running = 0;
}

void tcp_node_destroy(tcp_node_t tcp_node){

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
		tcp_connection_destroy(tcp_node->connections[i]);
	}
	// free the array itself
	free(tcp_node->connections);
	
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
	
		
	free(tcp_node);
	tcp_node = NULL;
}
/************** Functions dealing with Kernal Table  *************/


/* For accept */
/* - calls on the listening_connection to dequeue its triple and node creates new connection with information
     returned int is the new socket assigned to that new connection.  
   - The connection finishes its handshake to get to established state
   - Fills addr with ip address information from dequeued triple*/
int tcp_node_connection_accept(tcp_node_t tcp_node, tcp_connection_t listening_connection, struct in_addr *addr){
	
	if(tcp_connection_get_state(listening_connection) != LISTEN)
		return -EINVAL; //Socket is not listening for connections, or addrlen is invalid (e.g., is negative).
	
	// dequeue from accept_queue of listening connection to get triple of information about new connection
	accept_queue_triple_t triple = tcp_connection_accept_queue_dequeue(listening_connection);
	
	if(triple == NULL)
		return -1;
	
	// fill struct in_addr
	addr->s_addr = triple->remote_ip;
	
	// create new connection which will be the accepted connection 
	// -- function will insert it into kernal array and socket hashmap
	tcp_connection_t new_connection = tcp_node_new_connection(tcp_node);
	
	// assign values from triple to that connection
	tcp_connection_set_remote(new_connection, triple->remote_ip, triple->remote_port);
	tcp_connection_set_last_seq_received(new_connection, triple->last_seq_received);
	
	// assign new port to new_connection
	int port = tcp_node_next_port(tcp_node);
	tcp_node_assign_port(tcp_node, new_connection, port);
	
	// set state of this new_connection to LISTEN so that we can send it through transition LISTEN_to_SYN_RECEIVED
	tcp_connection_set_state(new_connection, LISTEN);
	
	// have connection transition from LISTEN to SYN_RECEIVED
	tcp_connection_state_machine_transition(new_connection, SYN_RECEIVED);
	
	// destroy triple
	accept_queue_triple_destroy(triple);
	
	return tcp_connection_get_socket(new_connection);
}
// creates a new tcp_connection and properly places it in kernal table -- ports and ips initialized to 0
tcp_connection_t tcp_node_new_connection(tcp_node_t tcp_node){
	// init new tcp_connection
	tcp_connection_t connection = tcp_connection_init(tcp_node_next_virt_socket(tcp_node), tcp_node->to_send);

	// place connection in array
	_insert_connection_array(tcp_node, connection);
	
	// place connection in hashmaps
	connection_virt_socket_keyed_t socket_keyed = connection_virt_socket_keyed_init(connection);
	HASH_ADD_INT(tcp_node->virt_socketToConnection, virt_socket, socket_keyed);

	return connection;
}

// returns tcp_connection corresponding to socket
tcp_connection_t tcp_node_get_connection_by_socket(tcp_node_t tcp_node, int socket){
	
	connection_virt_socket_keyed_t socket_keyed;
	HASH_FIND_INT(tcp_node->virt_socketToConnection, &socket, socket_keyed);
	if(!socket_keyed)
		return NULL;
	else
		return socket_keyed->connection;
}

// returns tcp_connection corresponding to port
tcp_connection_t tcp_node_get_connection_by_port(tcp_node_t tcp_node, uint16_t port){
	connection_port_keyed_t port_keyed;
	HASH_FIND_INT(tcp_node->portToConnection, &port, port_keyed);
	if(!port_keyed)
		return NULL;
	else
		return port_keyed->connection;
}

// assigns port to tcp_connection and puts entry in hash table that hashes ports to tcp_connections
// returns 1 if port successfully assigned, 0 otherwise
int tcp_node_assign_port(tcp_node_t tcp_node, tcp_connection_t connection, int port){
	
	// set connection's port
	uint16_t uport = (uint16_t)port;
	tcp_connection_set_local_port(connection, uport);
	
	// put port to connection in kernal
	connection_port_keyed_t port_keyed = connection_port_keyed_init(connection);
	HASH_ADD_INT(tcp_node->portToConnection, port, port_keyed);	
	
	return 1;
}

// returns 1 if the port is available for use, 0 if already in use
int tcp_node_port_unused(tcp_node_t tcp_node, int port){	
	
	connection_port_keyed_t port_keyed;
	// check that port not already in hashmap
	HASH_FIND_INT(tcp_node->portToConnection, &port, port_keyed);
	if(!port_keyed)
		return 1;
	else
		return -1;
}

// returns next available, currently unused, port to bind or connect/accept a new tcp_connection with
int tcp_node_next_port(tcp_node_t tcp_node){
	int next_port = tcp_node->port_count + 1;
	
	// check that next_port not already in use -- not already in hashmap	
	while(!tcp_node_port_unused(tcp_node, next_port)){
		next_port ++;
	}
	tcp_node->port_count = next_port;
	return next_port;
}

// returns next available, currently unused, virtual socket file descriptor to initiate a new tcp_connection with
int tcp_node_next_virt_socket(tcp_node_t tcp_node){
	int next_socket = (tcp_node->virt_socket_count)+1;
	tcp_node->virt_socket_count = next_socket;
	return next_socket;
}
/**************** End of functions that deal with Kernal Table *******************/

// iterate through sockets hash map to print info about each socket
void tcp_node_print(tcp_node_t tcp_node){

	//char* buffer = malloc(sizeof(char)*INET_ADDRSTRLEN);
	tcp_connection_t connection;
	
	connection_virt_socket_keyed_t socket_keyed, tmp;
	HASH_ITER(hh, tcp_node->virt_socketToConnection, socket_keyed, tmp){
		connection = socket_keyed->connection;
		tcp_connection_print_sockets(connection);
	}	
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
	while((tcp_node->running)&&(tcp_node_ip_running(tcp_node))){	

		/* get the time of the day so that we are passing in to bqueue_timed_dequeue_abs
			the absolute time when we want the timeout to occur (the docs in bqueue.c say
			relative, but then the argument is called abs_ts so its unclear what they intended).
			This works for now but we might need to change it when we port over to linux */
		gettimeofday(&now, NULL);	
		wait_cond.tv_sec = now.tv_sec+PTHREAD_COND_TIMEOUT_SEC;
		wait_cond.tv_nsec = 1000*now.tv_usec+PTHREAD_COND_TIMEOUT_NSEC;
		
		/* try to get the next thing on queue */
		ret = bqueue_timed_dequeue_abs(to_read, &packet, &wait_cond);
		if (ret != 0) 
			/* should probably check at this point WHY we failed (for instance perhaps the queue
				was destroyed */
			continue;
			
		/* otherwise there's a packet waiting for you! */
		_handle_packet(tcp_node, (tcp_packet_data_t)packet);
	}
	
	ip_node_stop(tcp_node->ip_node);

	int rc;
	rc = pthread_join(ip_link_interface_thread, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_join() is %d\n", rc);
		exit(-1);
	}
	rc = pthread_join(ip_send_thread, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_join() is %d\n", rc);
		exit(-1);
	}
	rc = pthread_join(ip_command_thread, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_join() is %d\n", rc);
		exit(-1);
	}
	rc = pthread_cancel(tcp_stdin_thread);
	if (rc) {
		printf("ERROR; return code from pthread_join() is %d\n", rc);
		exit(-1);
	}

	puts("finished.");
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



/* num_butes>0 indicates there is data and therefore to_write must be pushed to window 
int tcp_node_send(tcp_node_t tcp_node, char* to_write, int socket, uint32_t num_bytes){

	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(!connection)	
		return -EBADF;
	
	int ret;
	ret = tcp_connection_send(connection, to_write, num_bytes);
	return ret;
}*/

/*
tcp_node_invalid_port
	handles the case where a packet is received to which there is no open 
	port. Should inform the sender?
*/
void tcp_node_invalid_port(tcp_node_t tcp_node, tcp_packet_data_t packet){
	// let's just not do anything. That'll show em -- that's ridiculous
	puts("tcp_node_invalid_port -- are we going to handle this?? TODO");
	// this might be appropriate: ECONNREFUSED = No-one listening on the remote address.
}
	

/************************** Internal Functions Below ********************************************/

// inserts connection in array of connections -- grows the array if necessary
// returns number of connections in array
static int _insert_connection_array(tcp_node_t tcp_node, tcp_connection_t connection){
	
	int num_connections = tcp_node->num_connections;
	int connection_array_size = tcp_node->connection_array_size;
	
	if(num_connections+1 > connection_array_size){
		// need to create larger array of tcp_connections
		tcp_connection_t* orig_connections = tcp_node->connections;
		connection_array_size = connection_array_size*2;
		tcp_connection_t* new_connections = (tcp_connection_t*)realloc(tcp_node->connections, connection_array_size*sizeof(tcp_connection_t));
		// iterate through new array to put old function pointers into new array
		int i;
		for(i = 0; i<num_connections; i++){
			new_connections[i] = orig_connections[i];
		}
		// set new array 
		tcp_node->connections = new_connections;
		tcp_node->connection_array_size = connection_array_size;
	}
	// insert new connection in array
	tcp_node->connections[num_connections] = connection;
	tcp_node->num_connections = num_connections + 1;
	
	return tcp_node->num_connections;
}

static void _handle_packet(tcp_node_t tcp_node, tcp_packet_data_t tcp_packet){
	int packet_size = tcp_packet->packet_size;
	if( packet_size < TCP_HEADER_MIN_SIZE ){
		puts("packet received is less than header size, discarding...");
		free(tcp_packet);
		return;
	}

	/* validating the checksum happens in handle_receive_packet in tcp_connection,
		because it's dependent on the ip of the sender/receiver, and why does the
		node care if its gonna get discarded 
	
	if(tcp_utils_validate_checksum(tcp_packet->packet) < 0){
		puts("Bad checksum -- discarding packet");
		free(tcp_packet);
		return;
	}
	*/
		
	uint16_t dest_port   = tcp_dest_port(tcp_packet->packet);

	tcp_connection_t connection = tcp_node_get_connection_by_port(tcp_node, dest_port);
	if(!connection){
		tcp_node_invalid_port(tcp_node, tcp_packet);
		free(tcp_packet);
		return;
	}

	tcp_connection_handle_receive_packet(connection, tcp_packet);
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
/***************** FOR TESTING *********************/

uint32_t tcp_node_get_interface_remote_ip(tcp_node_t tcp_node, int interface_num){
	uint32_t ip_addr;
	ip_addr = ip_node_get_interface_remote_ip(tcp_node->ip_node, interface_num);
	return ip_addr;
}






