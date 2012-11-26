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
/* returns result */
int tcp_api_args_destroy(tcp_api_args_t* args){
	/* first join your thread. this will block
		if you're not done yet */ 
	tcp_connection_t connection = tcp_node_get_connection_by_socket((*args)->node, (*args)->socket);
	if(connection){
		//lets cancel any blocks on the api signal so that shutting down doesn't take a while
		tcp_connection_api_cancel(connection); 
		/* unlock */
		tcp_connection_api_unlock(connection);
	}
		
    pthread_join((*args)->thread, NULL);
    int result = (*args)->result;
	/* For this not to go wrong we had better set args->addr to NULL at first.  See function init() */
	if((*args)->addr != NULL)
		free((*args)->addr);
	if((*args)->buffer != NULL)
		free((*args)->buffer);
	free(*args);
	*args = NULL;
    return result;
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
	if(!connection)	
		return -EBADF;

	/* Make sure connection has a unique port before sending anything so that node can multiplex response */
	if(!tcp_connection_get_local_port(connection))
		tcp_node_assign_port(tcp_node, connection, tcp_node_next_port(tcp_node), 0);

	//connection needs to know both its local and remote ip before sending
	uint32_t local_ip = tcp_node_get_local_ip(tcp_node, (*addr).s_addr);

	if(!local_ip){
		return -ENETUNREACH;
	}
	
	tcp_connection_set_remote_ip(connection, (*addr).s_addr);
	tcp_connection_set_local_ip(connection, local_ip);
	
	// we could check if the transition is valid, but let's just not, instead it 
	// is handled and signaled just like everything else by a call to tcp_connection_invalid_transition
	int ret = tcp_connection_active_open(connection, tcp_connection_get_remote_ip(connection), port);
	if(ret==INVALID_TRANSITION){
		// then just return that result (well maybe lets return EBADF, is that right?);
		return -EBADF;//INVALID_TRANSITION;
	}
	
	/* Now wait until connection ESTABLISHED or timed out -- ret value will indicate */
	int transition_result = tcp_connection_api_result(connection); // will block until it gets the result
	//handle result
	if(transition_result < 0){
		// error or timeout so lets get rid of this
		tcp_node_remove_connection_kernal(tcp_node, connection); 
		if(transition_result == CONNECTION_RESET)
			return -ECONNREFUSED;
		else
			return transition_result;
	}
	if(ret==1) //successful active_open
		return 0;
	
	return ret;
}

/* write on an open socket (SEND in the RFC)
return num bytes written or negative number on failure */
int tcp_api_write(tcp_node_t tcp_node, int socket, const unsigned char* to_write, uint32_t num_bytes){

	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(!connection)	
		return -EBADF;
	
	int ret;
	ret = tcp_connection_send_data(connection, to_write, num_bytes);
	return ret;
}

void* tcp_api_sendfile_entry(void* _args){
	tcp_api_args_t args = (tcp_api_args_t) _args;
	
	/* verify that the necessary args were set */
	_verify_node(args);
	_verify_socket(args);
	_verify_addr(args);
	_verify_buffer(args);
	_verify_port(args);
	
	/* lock it up */	
	tcp_connection_t connection = tcp_node_get_connection_by_socket(args->node, args->socket);
	if(connection == NULL){	
		_return(args, -EBADF); 	 // = The file descriptor is not a valid index in the descriptor table.
		return NULL;
	}
	tcp_connection_api_lock(connection);// make sure no one else is messing with the socket/connection
		
	/* open file so we can verify valid before we open any connections that we'll then need to close */
	FILE* f = fopen(args->buffer, "r");
	if(!f){
		fprintf(stderr, "Unable to open given file: %s\n", args->buffer);
		_return(args, -EINVAL);	//Invalid argument passed
		return NULL;
	}
	
	/* open connection */
	int ret = tcp_api_connect(args->node, args->socket, args->addr, args->port);
	if(ret<0){
		args->function_call = "sendfile: v_socket()";
		_return(args, ret);
		return NULL;		
 	}
	
	char input_line[BUFFER_SIZE];
	while(fgets(input_line, BUFFER_SIZE-1, f)){
		ret = tcp_connection_send_data(connection, (unsigned char*)input_line, strlen(input_line));
		if (ret < 0){
			args->function_call = "sendfile: v_write()";
			_return(args, ret);
			return NULL;
		}
	}
	
	//clean up
	fclose(f);
	// close connection we opened 
	tcp_api_close(args->node, args->socket); //locks and blocks but we don't need this anymore anyhow
	/* and use my macro to return it 
		(first arg is size of retal) */
	_return(args, 0);
	return NULL;
}
void* tcp_api_recvfile_entry(void* _args){
	tcp_api_args_t args = (tcp_api_args_t) _args;

	/* verify that the necessary args were set */
	_verify_node(args);
	_verify_socket(args);
	_verify_buffer(args);
	_verify_port(args);
	
	/* lock it up */	
	tcp_connection_t connection = tcp_node_get_connection_by_socket(args->node, args->socket);
	if(connection == NULL){	
		_return(args, -EBADF); 	 // = The file descriptor is not a valid index in the descriptor table.
		return NULL;
	}
	tcp_connection_api_lock(connection);// make sure no one else is messing with the socket/connection

/* OPEN THE FILE */
	/* open file so we can verify valid before we open any connections that we'll then need to close */
	FILE* f = fopen(args->buffer, "w");
	if(!f){
		fprintf(stderr, "Unable to open file for writing: %s\n", args->buffer);
		_return(args, -EINVAL);	//Invalid argument passed
		return NULL;
	}
	/* Get new accepted connection -- this call will block */
	struct in_addr *addr;	
	int reading_socket = tcp_api_accept(args->node, args->socket, addr);
	if(reading_socket < 0){
		_return(args, reading_socket);
		return NULL;
	}
	tcp_connection_t reading_connection = tcp_node_get_connection_by_socket(args->node, reading_socket);
	if(reading_connection == NULL){	
		puts("ERROR: Bug: See recvfile_entry");
		_return(args, -EBADF); 	 // = The file descriptor is not a valid index in the descriptor table.
		return NULL;
	}	
	
	while(tcp_node_running(args->node) && tcp_connection_get_state(reading_connection) != CLOSE_WAIT){
	
	}

/* CLEAN UP */
	// close connections we opened 
	tcp_api_close(args->node, reading_socket);
	tcp_api_close(args->node, args->socket); //locks and blocks but we don't need this anymore anyhow
	// clean up the file
	fclose(f);

	/* and use my macro to return it 
		(first arg is size of retal) */
	_return(args, 0);
	return NULL;
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

	/* lock it up */	
	tcp_connection_t connection = tcp_node_get_connection_by_socket(args->node, args->socket);
	if(connection == NULL){
		_return(args,-EBADF); 	 // = The file descriptor is not a valid index in the descriptor table.
		return NULL;
	}
	tcp_connection_api_lock(connection);// make sure no one else is messing with the socket/connection
	
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
int tcp_api_bind(tcp_node_t tcp_node, int socket, struct in_addr* addr, uint16_t port){

	// check if port already in use
	if(tcp_node_port_unused(tcp_node, port, 0) < 0)		
		return -EADDRINUSE;	//The given address is already in use.

	// get corresponding tcp_connection
	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)
		return -EBADF; 	//socket is not a valid descriptor
	
	/* We don't call this in a thread, so no need to block */
	
	if(tcp_connection_get_local_port(connection)){
		return -EINVAL; 	// The socket is already bound to an address.
	}
	tcp_node_assign_port(tcp_node, connection, port, 0);
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
		tcp_node_assign_port(tcp_node, connection, port, 0);
	}
	
	if(tcp_connection_passive_open(connection) < 0){ // returns -1 on failure
		return -EBADF;
	}
	
	port = (int)tcp_connection_get_local_port(connection);
	
	return port; // returns 0 on success
}	

/* read on an open socket (RECEIVE in the RFC)
return num bytes read or negative number on failure or 0 on eof */
//int v read(int socket, unsigned char *buf, uint32 t nbyte);
int tcp_api_read(tcp_node_t tcp_node, int socket, char *buffer, uint32_t nbyte){
	print(("tcp_api_read 0"), ALEX_PRINT);
	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)
		return -EBADF;

	/* Don't lock because tcp_api read_entry locks up -- it needs to lock up because its the blocking call */

	if(tcp_connection_get_recv_window(connection) == NULL || (!tcp_connection_recv_window_alive(connection))){
		// probably means we called v_shutdown type 2 to close reading portion of socket -- return error
		printf("[Socket %d]: Illegal read call\n", tcp_connection_get_socket(connection));
		return -1;
	}

	state_e state = tcp_connection_get_state(connection);
	if(state == CLOSED || state == LAST_ACK){
		return 0;
	}
	
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
	return read;
}

void* tcp_api_read_entry(void* _args){
	tcp_api_args_t args = (tcp_api_args_t)_args;

	/* verifies that these fields are valid (node != NULL, socket >=0, ...) */
	_verify_node(args);
	_verify_socket(args);
	
	tcp_connection_t connection = tcp_node_get_connection_by_socket(args->node, args->socket);
	if(connection == NULL){
		_return(args,-EBADF);
		return NULL;
	}
	tcp_connection_api_lock(connection);
	
	/* I think this is the correct place to check state because if we're in the connect/accept state for example,
		we might already be blocking and then it wouldn't make sense to call lock on the connection */
	
	//tacked on an extra 1 for null character for pretty print
	char* to_read = (char*)malloc(sizeof(char)*(args->num + 1));
	//char to_read[args->num + 1];

	int ret = tcp_api_read(args->node, args->socket, to_read, args->num);	

	//TODO: HANDLE CORRECT RESPONSES BASED ON STATE
	// CAN continue to read in the FIN-WAIT-1 state	
	state_e state = tcp_connection_get_state(connection);	
	
	if(ret == 0){ 
		// was there nothing to read, or did connection close? let's check
		if(state == CLOSED || state == CLOSE_WAIT || state == LAST_ACK){
			puts("Remote Connection Closed");
			free(to_read);
			//inform application layer that we need to close -- is this the right way to do it?
			_return(args,0);	//return 0?
			return NULL;
		}
	}	
	if(ret<0){
		free(to_read);
		_return(args, ret);
		return NULL;
	}
	
	if(args->boolean){
		// block until read in args->num bytes
		int read = ret;	
		while(ret < args->num){
			if(read < 0){
				free(to_read);
				_return(args, read);
				return NULL;
			}
			if(read == 0){
				if(state == CLOSED || state == CLOSE_WAIT || state == LAST_ACK){
					break; //remote connection closed, but lets still print what we got til then
				}
				// need to wait until there is something to read
				int result = tcp_connection_api_result(connection); // will block until it gets the result

				if(result<0){
					free(to_read);
					if(result == REMOTE_CONNECTION_CLOSED){
						_return(args, 0); //return 0 to signify remote connection closed
						return NULL;
					}
					_return(args, result);
					return NULL;
				}
			}
			read = tcp_api_read(args->node, args->socket, to_read+ret, (args->num)-ret);	
			ret = ret + read;
		}
	}

	// NOTE! You can't just print the buffer because it's not null-teriminated!
	// On mac's this will be no problem, because the memory is nicely 0-ed out 
	// for us, on linux this won't be the case  <-- k thanx
	to_read[ret] = '\0';
	printf("[read for socket %d]:\n\t%s\n", args->socket, to_read); 
	if(state == CLOSED || state == CLOSE_WAIT || state == LAST_ACK)
		printf("[Socket %d]: Remote Connection Closed\n", args->socket);
	free(to_read);
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

	while(!tcp_connection_get_close_boolean(listening_connection)){ //if first connection doesn't become all the way ESTABLISHED, want to repeat
	
		
		/* listening connection must actually be listening */
		if(tcp_connection_get_state(listening_connection) != LISTEN){
			return -EINVAL; //Socket is not listening for connections, or addrlen is invalid (e.g., is negative).
		}	
	
		/* calls on the listening_connection to dequeue its triple and node creates new connection with information
		 new socket is the socket assigned to that new connection.  This connection will then go on to finish
		 the three-way handshake to reach ESTABLISHED state */
		/* THIS CALL IS BLOCKING -- because the accept_queue is a bqueue -- call returns when accept_data_t dequeued */
		tcp_connection_t new_connection = tcp_node_connection_accept(tcp_node, listening_connection);
		if(new_connection == NULL){
			puts("tcp_api_accept: NULL\n");	
			// NULL is returned when we've reached max number of file descriptors or trying to close
			if(tcp_connection_get_close_boolean(listening_connection)) // we were just rying to close
				return CONNECTION_CLOSED;
			return -ENFILE;	//The system limit on the total number of open files has been reached.
		}
		printf("tcp_api_accept: socket: %d\n", tcp_connection_get_socket(new_connection));	

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
			return SIGNAL_DESTROYING;
		}
		else if(ret == API_TIMEOUT){ //changing from SYN_RECEIVED to ESTABLISHED timed out
			
			puts("tcp_api_accept: API_TIMEOUT");	
			// want to close it but don't want to block -- let's thread the close??! (which will also remove it)
			tcp_api_args_t args = tcp_api_args_init();
			args->node = tcp_node;
			args->socket = tcp_connection_get_socket(new_connection);
			args->function_call = "v_close";		
			tcp_node_thread(tcp_node, tcp_api_close_entry, args);
		
			continue; //try again
		}
		else if(ret == REMOTE_CONNECTION_CLOSED){	//Instead of sending back ack they sent back fin
			puts("tcp_api_accept: REMOTE_CONNECTION_CLOSED");
			// lets close this connection responsibly and try again
			tcp_api_args_t args = tcp_api_args_init();
			args->node = tcp_node;
			args->socket = tcp_connection_get_socket(new_connection);
			args->function_call = "v_close";		
			tcp_node_thread(tcp_node, tcp_api_close_entry, args);
			
			continue; //try again
		}	
	
		/* Our connection has been established! 
		TODO:HANDLE bad ret value */		
		return ret;	
 	} //end of while loop which allowed us to call continue
 	return 0; //I guess someone tried to close
}
// Not for driver use -- just for our use when we only want to accept once
void* tcp_api_accept_entry(void* _args){
	tcp_api_args_t args = (tcp_api_args_t)_args;

	// verifies that these fields are valid (node != NULL, socket >=0, ...) 
	_verify_node(args);
	_verify_socket(args);

	tcp_connection_t connection = tcp_node_get_connection_by_socket(args->node, args->socket);
	if(connection == NULL)
		_return(args, -EBADF);
	
	tcp_connection_api_lock(connection);
	
	struct in_addr addr;
	// blocks until gets new connection or bad value
	int ret = tcp_api_accept(args->node, args->socket, &addr);

	_return(args, ret);
	return NULL;
}

////////////////// DRIVER VERSION///////////////////////

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
		
	// before we call this a million times, lets first make sure everything's valid
	tcp_connection_t listening_connection = tcp_node_get_connection_by_socket(args->node, args->socket);
	if(listening_connection == NULL){
		_return(args, -EBADF);
		return NULL; // this won't do anything
	}
	tcp_connection_api_lock(listening_connection);
	
	int ret;
	while(tcp_node_running(args->node)&&!tcp_connection_get_close_boolean(listening_connection)){
		
		struct in_addr addr;
		// blocks until gets new connection or bad value
		ret = tcp_api_accept(args->node, args->socket, &addr);
		if(!(tcp_node_running(args->node)) || !tcp_connection_get_close_boolean(listening_connection)){
			ret = 0;
			break; //we might have broken out with an error value because tcp_node started destroying stuff already
		}
		if(ret<0)
			break;
		
		printf("v_accept() returned socket: %d\n", ret);
	}
	if(ret == CONNECTION_CLOSED)
		ret = 0;
	
	//* we'll use the macro _return in order to return a value * //<--- nah lets have it just be successful
	_return(args, ret);
	return NULL; // this won't do anything
}

//////////////////////////////////////////////////////////////////////////////////////
/*********************************** CLOSING *****************************************/

/* Invalidate this socket, making the underlying connection inaccessible to
any of these API functions. If the writing part of the socket has not been
shutdown yet, then do so. The connection shouldn't be terminated, though;
any data not yet ACKed should still be retransmitted. */
int tcp_api_close(tcp_node_t tcp_node, int socket){
	
	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL){
		return -EBADF;
	}
	int ret;
		
	// CLOSE and close reading part
	ret = tcp_api_shutdown(tcp_node, socket, 3);

	if(ret == 0 && (tcp_connection_get_state(connection)!=CLOSED)) //success
		/* everything's going well and all, but we're still in the process of closing so let's not delete
		this connection until it has finished closing with its peer */
		ret = tcp_connection_api_result(connection);

	/* invalidate socket -- delete TCB 
		tcp_api_args checks that connection not null before calling unlock so no worries about deleting it */	
	tcp_node_remove_connection_kernal(tcp_node, connection);
	if(ret < 0) //error
		return ret;
	return 0; //success	
}
void* tcp_api_close_entry(void* _args){
	tcp_api_args_t args = (tcp_api_args_t)_args;

	/* verifies that these fields are valid (node != NULL, socket >=0, ...) */
	_verify_node(args);
	_verify_socket(args);

	tcp_connection_t connection = tcp_node_get_connection_by_socket(args->node, args->socket);
	if(connection == NULL){
		_return(args, -EBADF);
	}
	// sets the closing boolean now so that locking accept can unlock and return and then we can close yay
	tcp_connection_set_close(connection);
	// WE LOCK HERE AND UNLOCK IN TCP_API_ARGS_DESTROY which will check if connection null or not before calling unlock	
	tcp_connection_api_lock(connection);
		
	int ret = tcp_api_close(args->node, args->socket);

	_return(args, ret);
	return NULL;
}
/* shutdown an open socket. If type is 1, close the writing part of
the socket (CLOSE call in the RFC. This should send a FIN, etc.)
If 2 is speciﬁed, close the reading part (no equivalent in the RFC;
v read calls should just fail, and the window size should not grow any
more). If 3 is speciﬁed, do both. The socket is not invalidated.
returns 0 on success, or negative number on failure
If the writing part is closed, any data not yet ACKed should still be retransmitted. */
int tcp_api_shutdown(tcp_node_t tcp_node, int socket, int type){

	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)
		return -EBADF;
		
	int ret;
	
	if(type == SHUTDOWN_WRITE){
		ret = tcp_connection_close(connection);		
		if(ret < 0) //error
			return ret;	
		return 0; //success
	}
	else if(type == SHUTDOWN_READ){
		/* just close reading capability */
		ret = tcp_connection_close_recv_window(connection);
		if(ret < 0) //error
			return ret;
		return 0; //success
	}
	else if(type == SHUTDOWN_BOTH){
		/* close reading capability */
		tcp_connection_close_recv_window(connection);
		/* CLOSE */
		ret = tcp_connection_close(connection);
		if(ret < 0)
			return ret;
		return 0; //success
	}

	CRASH_AND_BURN("invalid option for v_shutdown");
	return -1;
}

void* tcp_api_shutdown_entry(void* _args){
	tcp_api_args_t args = (tcp_api_args_t)_args;

	/* verifies that these fields are valid (node != NULL, socket >=0, ...) */
	_verify_node(args);
	_verify_socket(args);	
	int type = args->num;
	
	tcp_connection_t connection = tcp_node_get_connection_by_socket(args->node, args->socket);
	if(connection == NULL){
		_return(args,-EBADF);
		return NULL;
	}
	if(type == 1 || type == 3){
		// sets the closing boolean now so that locking accept can unlock and return and then we can close yay
		tcp_connection_set_close(connection);
	}
	//WE LOCK HERE AND UNLOCK IN TCP_API_ARGS_DESTROY
	tcp_connection_api_lock(connection);
	
	int ret = tcp_api_shutdown(args->node, args->socket, type);
	
	_return(args, ret);
	return NULL;
}


