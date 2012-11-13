//api file
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#include "tcp_api.h"
#include "tcp_connection_state_machine_handle.h"
#include "recv_window.h" // for the read function

/* args */

tcp_api_args_t tcp_api_args_init(){
	tcp_api_args_t args = malloc(sizeof(struct tcp_api_args));
	memset(args, 0, sizeof(struct tcp_api_args));
	args->done = 0;
	args->addr = NULL;
	args->buffer = NULL;
	return args;
}

void tcp_api_args_destroy(tcp_api_args_t* args){
	/* first join your thread. this will block
		if you're not done yet */
	pthread_join((*args)->thread, NULL);
	/* For this not to go wrong we had better set args->addr to NULL at first.  See function init() */
	if((*args)->addr != NULL)
		free((*args)->addr);
	if((*args)->buffer != NULL)
		free((*args)->buffer);
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
#define _verify_buffer(args) if((args)->buffer == NULL) {CRASH_AND_BURN("NULL BUFFER TO READ INTO");}
#define _verity_bool(args)	 if((((args)->boolean)!=0)&&(((args)->boolean)!=1)) {CRASH_AND_BURN("INVALID BOOLEAN VALUE");}

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
		tcp_connection_api_unlock(connection);
		return -ENETUNREACH;
	}
	
	tcp_connection_set_remote_ip(connection, (*addr).s_addr);
	tcp_connection_set_local_ip(connection, local_ip);
	
	// we could check if the transition is valid, but let's just not, instead it 
	// is handled and signaled just like everything else by a call to tcp_connection_invalid_transition
	int ret = tcp_connection_active_open(connection, tcp_connection_get_remote_ip(connection), port);
	if(ret==INVALID_TRANSITION){
		// then just return that result (well maybe lets return EBADF, is that right?);
		tcp_connection_api_unlock(connection);
		return -EBADF;//INVALID_TRANSITION;
	}
	
	/* Now wait until connection ESTABLISHED or timed out -- ret value will indicate */
	int transition_result = tcp_connection_api_result(connection); // will block until it gets the result
	//unlock and handle result
	tcp_connection_api_unlock(connection);
	if(transition_result < 0){
		// error or timeout so lets get rid of this
		tcp_node_remove_connection_kernal(tcp_node, connection); 
		return transition_result;
	}
	if(ret==1) //successful active_open
		return 0;
	
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
//int v bind(int socket, struct in addr addr, uint16 t port);
int tcp_api_bind(tcp_node_t tcp_node, int socket, struct in_addr addr, uint16_t port){

	// check if port already in use
	if(tcp_node_port_unused(tcp_node, port) < 0)		
		return -EADDRINUSE;	//The given address is already in use.

	// get corresponding tcp_connection
	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)
		return -EBADF; 	//socket is not a valid descriptor
	
	/* We don't call this in a thread, so no need to block */
	
	if(tcp_connection_get_local_port(connection)){
		return -EINVAL; 	// The socket is already bound to an address.
	}
	tcp_node_assign_port(tcp_node, connection, port);
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

/* read on an open socket (RECEIVE in the RFC)
return num bytes read or negative number on failure or 0 on eof */
//int v read(int socket, unsigned char *buf, uint32 t nbyte);
int tcp_api_read(tcp_node_t tcp_node, int socket, char *buffer, uint32_t nbyte){

	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)
		return -EBADF;

	/* Lock up api on this connection -- BLOCK  -- really we don't want to 
		be able to call this if another api call in process */
	tcp_connection_api_lock(connection);
	
	state_e state = tcp_connection_get_state(connection);
	
	//TODO: HANDLE CORRECT RESPONSES BASED ON STATE
	
	if(state == CLOSE_WAIT){
		tcp_connection_api_unlock(connection);
		return 0; //inform application layer that we need to close
	}
	
	if(tcp_connection_get_state(connection) != ESTABLISHED){
		//TODO: HANDLE APPROPRIATELY
		tcp_connection_api_unlock(connection);
		return -1; //<-- get correct error code
	}
/*
struct memchunk{
	void* data;
	int length;
};*/
	memchunk_t chunk = recv_window_get_next(tcp_connection_get_recv_window(connection), nbyte);
	if(!chunk){
		return 0;
	}	
	int read = nbyte;
	if(chunk->length > nbyte){
		puts("Error: Alex and Neil go debug tcp_api_read");
		exit(-1);
	}
	if(chunk->length < nbyte)
		read = chunk->length;
	memcpy(buffer, chunk->data, read); 
	
	//clean up
	memchunk_destroy_total(&chunk, util_free);
	tcp_connection_api_unlock(connection);
	
	return read;
}
void* tcp_api_read_entry(void* _args){
	tcp_api_args_t args = (tcp_api_args_t)_args;

	/* verifies that these fields are valid (node != NULL, socket >=0, ...) */
	_verify_node(args);
	_verify_socket(args);
	_verify_buffer(args);
	
	if(args->num <= 0){
		_return(args, 0);
		return NULL;
	}
	
	/* we'll use the macro thread_return in order to return a value */
	
	int ret = tcp_api_read(args->node, args->socket, (char*)args->buffer, args->num);
	if(args->boolean){
		// block until read in args->num bytes
		int read;	
		while(ret < args->num){
			// ok but if the window is empty this will give a really
			// draining infinite loop
			read = tcp_api_read(args->node, args->socket, (char*)(args->buffer)+ret, (args->num)-ret);
			if(read < 0){
				_return(args, read);
				return NULL;
			}
	
			ret = ret + read;
		}
	}

	// NOTE! You can't just print the buffer because it's not null-teriminated!
	// On mac's this will be no problem, because the memory is nicely 0-ed out 
	// for us, on linux this won't be the case
	char buffer[ret+1];
	memcpy(buffer, args->buffer, ret);
	buffer[ret] = '\0';

	printf("[read for socket %d]:\n\t%s\n", args->socket, buffer); 
	_return(args, ret);
	return NULL;
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
	tcp_connection_t new_connection = tcp_node_connection_accept(tcp_node, listening_connection);
	if(new_connection == NULL){
		// NULL is returned when we've reached max number of file descriptors
		tcp_connection_api_unlock(listening_connection);
		return -ENFILE;	//The system limit on the total number of open files has been reached.
	}

	// set state of this new_connection to LISTEN so that we can send it through transition LISTEN_to_SYN_RECEIVED
	tcp_connection_set_state(new_connection, LISTEN);
	
	// have connection transition from LISTEN to SYN_RECEIVED
	if(tcp_connection_state_machine_transition(new_connection, receiveSYN)<0)
		CRASH_AND_BURN("Alex and Neil go debug: tcp_connection_state_machine_transition(new_connection, receiveSYN)) returned negative value in tcp_node_connection_accept");
	
	//now set addr appropriately
	addr->s_addr = tcp_connection_get_remote_ip(new_connection);
	
	/* Now wait until connection ESTABLISHED 
		when established connection should call tcp_api_accept_help which will signal the accept_cond */
	int ret = tcp_connection_api_result(new_connection); // if successful = new_connection->socket_id;
	if(ret == SIGNAL_DESTROYING){
		// is there anything else we can do here?
		tcp_connection_api_unlock(listening_connection);
		return SIGNAL_DESTROYING;
	}

	tcp_connection_api_unlock(listening_connection);

	/* Our connection has been established! 
	TODO:HANDLE bad ret value */
	
	return ret;	
 
}
// Not for driver use -- just for our use when we only want to accept once
void* tcp_api_accept_entry(void* _args){
	tcp_api_args_t args = (tcp_api_args_t)_args;

	// verifies that these fields are valid (node != NULL, socket >=0, ...) 
	_verify_node(args);
	_verify_socket(args);
	
	struct in_addr addr;
	// blocks until gets new connection or bad value
	int ret = tcp_api_accept(args->node, args->socket, &addr);

	_return(args, ret);
	return NULL;
}

////////////////// DRIVER ///////////////////////

/* So we need to call tcp_api_accept in a loop without blocking.... so here's my solution that I've implemented:
	We call thread the call tcp_driver_accept_entry which then goes to call tcp_api_accept_entry in a loop.
	This means we're creating threads within the while loop thread, but this way we can pretty print the result
	of each individual tcp_api_accept call.  the thread for tcp_driver_accept_entry has return value 0
	because the actual accept calls that returned sockets will have already been printed.
*/
void* tcp_driver_accept_entry(void* _args){ 
	
	tcp_api_args_t args = (tcp_api_args_t)_args;
	/* verifying fields */
	_verify_node(args);
	_verify_socket(args);
	
	tcp_node_t tcp_node = args->node;
		
	// before we call this a million times, lets first make sure everything's valid
	tcp_connection_t listening_connection = tcp_node_get_connection_by_socket(tcp_node, args->socket);
	if(listening_connection == NULL){
		_return(args, -EBADF);
		return NULL; // this won't do anything
	}
	
	int ret;
	while(tcp_node_running(args->node)){
		
		struct in_addr addr;
		// blocks until gets new connection or bad value
		ret = tcp_api_accept(args->node, args->socket, &addr);
		if(!(tcp_node_running(args->node))){
			ret = 0;
			break; //we might have broken out with an error value because tcp_node started destroying stuff already
		}
		if(ret<0)
			break;
		
		printf("v_accept() returned socket: %d\n", ret);
	}

	//* we'll use the macro _return in order to return a value * //<--- nah lets have it just be successful
	_return(args, ret);
	return NULL; // this won't do anything
}


