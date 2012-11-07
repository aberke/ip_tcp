//api file
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#include "tcp_api.h"
#include "tcp_connection_state_machine_handle.h"

/* args */

tcp_api_args_t tcp_api_args_init(){
	tcp_api_args_t args = malloc(sizeof(struct tcp_api_args));
	memset(args, 0, sizeof(struct tcp_api_args));
	args->done = 0;
	return args;
}

void tcp_api_args_destroy(tcp_api_args_t* args){
	/* first join your thread. this will block
		if you're not done yet */
	pthread_join((*args)->thread, NULL);

	/* for now just free it, but should probably also destroy the struct in_addr* */
	free(*args);
	*args = NULL;
}

//// these verify that the desired field is present and valid in arguments
//// just FYI, crash_and_burn() is defined in utils/utils.h and it does the
//// required crashing and/or burning (ie exit)
#define _verify_node(args) if((args)->node == NULL) {CRASH_AND_BURN("INVALID NODE");}
#define _verify_socket(args) if((args)->socket < 0) {CRASH_AND_BURN("INVALID SOCKET");}
#define _verify_addr(args) if((args)->addr == NULL) {CRASH_AND_BURN("INVALID ADDR");}
#define _verify_port(args) // always true

static void* _return(tcp_api_args_t args, int ret){
	//puts("thread queueing return");
	args->done = 1;
	args->result = ret;
	pthread_exit(NULL);
}


/* connects a socket to an address (active OPEN in the RFC)
returns 0 on success or a negative number on failure */

int tcp_api_connect(tcp_node_t tcp_node, int socket, struct in_addr* addr, uint16_t port){

	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)	
		return -EBADF; 	 // = The file descriptor is not a valid index in the descriptor table.
	
	tcp_connection_api_lock(connection);// make sure no one else is messing with the socket/connection

	/* Make sure connection has a unique port before sending anything so that node can multiplex response */
	if(!tcp_connection_get_local_port(connection))
		tcp_node_assign_port(tcp_node, connection, tcp_node_next_port(tcp_node));
	
	//connection needs to know both its local and remote ip before sending
	uint32_t local_ip = tcp_node_get_local_ip(tcp_node, (*addr).s_addr);
	
	if(!local_ip){
		return -ENETUNREACH;
	}
	
	tcp_connection_set_remote_ip(connection, (*addr).s_addr);
	tcp_connection_set_local_ip(connection, local_ip);
	
	tcp_connection_active_open(connection, tcp_connection_get_remote_ip(connection), port);
	
	/* Now wait until connection ESTABLISHED or timed out -- ret value will indicate */
	int ret = tcp_connection_api_result(connection); // will block until it gets the result
	if(ret == SIGNAL_DESTROYING){
		// is there anything else we should do here?
		return SIGNAL_DESTROYING;
	}

	return ret;
}

/* entry function for letting the above function be called by a thread

	design defense: this way tcp_api_connect doesn't need to know that 
					it is being called in a new thread, it is just a function, 
					and we happen to be calling it in a thread that we spawned
					for just this purpose
*/
void* tcp_api_connect_entry(void* _args){
	tcp_api_args_t args = (tcp_api_args_t) _args;
	
	/* verify that the necessary args were set */
	_verify_node(args);
	_verify_socket(args);
	_verify_addr(args);
	_verify_port(args);

	/* then get the result */
	int ret = tcp_api_connect(args->node, args->socket, args->addr, args->port);

	/* and use my macro to return it 
		(first arg is size of retal) */
	_return(args, ret);
	return NULL;
}

// called by v_socket 	
int tcp_api_socket(tcp_node_t tcp_node){
	tcp_connection_t connection = tcp_node_new_connection(tcp_node);
	if(connection == NULL)
		return -ENFILE;//The system limit on the total number of open files has been reached.
	int socket = tcp_connection_get_socket(connection);
	return socket;
}

/* binds a socket to a port
always bind to all interfaces - which means addr is unused.
returns 0 on success or negative number on failure */
int tcp_api_bind(tcp_node_t tcp_node, int socket, char* addr, uint16_t port){

	// check if port already in use
	if(!tcp_node_port_unused(tcp_node, port))		
		return -EADDRINUSE;	//The given address is already in use.

	// get corresponding tcp_connection
	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)
		return -EBADF; 	//socket is not a valid descriptor
	/* Lock up api on this connection -- BLOCK  -- really we don't want to 
		be able to call this if another api call in process */
	///TODO
	
	if(tcp_connection_get_local_port(connection)){
		int port = tcp_connection_get_local_port(connection);
		return -EINVAL; 	// The socket is already bound to an address.
	}

	tcp_node_assign_port(tcp_node, connection, port);
	
	/* All done so unlock */
	//TODO
	
	return 0;
}

// returns port that connection is listening on, negative number on failure
int tcp_api_listen(tcp_node_t tcp_node, int socket){

	int port;

	// get corresponding tcp_connection
	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)
		return -EBADF; 	//socket is not a valid descriptor

	if(!tcp_connection_get_local_port(connection)){
		// port not already set -- must bind to random port	
		port = tcp_node_next_port(tcp_node);
		tcp_node_assign_port(tcp_node, connection, port);
	}
	
	if(tcp_connection_passive_open(connection) < 0){ // returns -1 on failure
		return -1;
	}
	
	port = (int)tcp_connection_get_local_port(connection);
	
	return port; // returns 0 on success
}	

/* accept a requested connection (behave like unix socket’s accept)
returns new socket handle on success or negative number on failure 
int v accept(int socket, struct in addr *node); */
int tcp_api_accept(tcp_node_t tcp_node, int socket, struct in_addr *addr){

	tcp_connection_t listening_connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(listening_connection == NULL)
		return -EBADF;

	/* Lock up api on this connection -- BLOCK  -- really we don't want to 
		be able to call this if another api call in process */
	tcp_connection_api_lock(listening_connection);
	
	/* listening connection must actually be listening */
	if(tcp_connection_get_state(listening_connection) != LISTEN){
		tcp_connection_api_unlock(listening_connection);
		return -EINVAL; //Socket is not listening for connections, or addrlen is invalid (e.g., is negative).
	}	

	/* calls on the listening_connection to dequeue its triple and node creates new connection with information
	 new socket is the socket assigned to that new connection.  This connection will then go on to finish
	 the three-way handshake to reach ESTABLISHED state */
	/* THIS CALL IS BLOCKING -- because the accept_queue is a bqueue -- call returns when accept_data_t dequeued */
	tcp_connection_t new_connection = tcp_node_connection_accept(tcp_node, listening_connection, addr);
	if(new_connection == NULL){
		tcp_connection_api_unlock(listening_connection);
		return -1; //TODO -- HANDLE BETTER -- WHICH ERROR CODE? WHAT HAPPENED?
	}

	// set state of this new_connection to LISTEN so that we can send it through transition LISTEN_to_SYN_RECEIVED
	tcp_connection_set_state(new_connection, LISTEN);
	
	// have connection transition from LISTEN to SYN_RECEIVED
	if(tcp_connection_state_machine_transition(new_connection, receiveSYN)<0)
		CRASH_AND_BURN("Alex and Neil go debug: tcp_connection_state_machine_transition(new_connection, receiveSYN)) returned negative value in tcp_node_connection_accept");
	
	
	/* Now wait until connection ESTABLISHED 
		when established connection should call tcp_api_accept_help which will signal the accept_cond */
	int ret = tcp_connection_api_result(new_connection); // if successful = new_connection->socket_id;
	if(ret == SIGNAL_DESTROYING){
		// is there anything else we can do here?
		return SIGNAL_DESTROYING;
	}

	tcp_connection_api_unlock(listening_connection);

	/* Our connection has been established! 
	TODO:HANDLE bad ret value */
	
	return ret;	
 
}

void* tcp_api_accept_entry(void* _args){
	tcp_api_args_t args = (tcp_api_args_t)_args;

	/* verifies that these fields are valid (node != NULL, socket >=0, ...) */
	_verify_node(args);
	_verify_socket(args);
	_verify_addr(args);
	
	/* we'll use the macro thread_return in order to return a value */
	int ret = tcp_api_accept(args->node, args->socket, args->addr);
	_return(args, ret);
	return NULL;
}

