//api file
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#include "tcp_api.h"
#include "tcp_connection_state_machine_handle.h"



/* connects a socket to an address (active OPEN in the RFC)
returns 0 on success or a negative number on failure */
void* tcp_api_connect(tcp_node_t tcp_node, int socket, struct in_addr addr, uint16_t port){
	
	pthread_mutex_t mtx;
	pthread_cond_t cond;
	
	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)	
		return -EBADF; 	 // = The file descriptor is not a valid index in the descriptor table.
	
	/* Lock up api on this connection -- BLOCK */
	mtx = tcp_connection_get_api_mutex(connection);
	cond = tcp_connection_get_api_cond(connection);
	pthread_mutex_lock(&mtx);
	
	/* Make sure connection has a unique port before sending anything so that node can multiplex response */
	if(!tcp_connection_get_local_port(connection))
		tcp_node_assign_port(tcp_node, connection, tcp_node_next_port(tcp_node));
	
	//connection needs to know both its local and remote ip before sending
	uint32_t local_ip = tcp_node_get_local_ip(tcp_node, (addr.s_addr));
	
	if(!local_ip)
		return -ENETUNREACH;
	
	tcp_connection_set_remote_ip(connection, addr.s_addr);
	tcp_connection_set_local_ip(connection, local_ip);
	
	tcp_connection_active_open(connection, tcp_connection_get_remote_ip(connection), port);
	
	/* Now wait until connection ESTABLISHED or timed out -- ret value will indicate */
	pthread_cond_wait(&cond, &mtx);

	/* After we return from that get ret value and return */
	int ret = tcp_connection_get_api_ret(connection); // should be 0 if successful
	
	/* Unlock for other api calls */
	pthread_mutex_unlock(&mtx);
	
	return ret;
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
	
	if(tcp_connection_get_local_port(connection))
		return -EINVAL; 	// The socket is already bound to an address.

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
void* tcp_api_accept(tcp_node_t tcp_node, int socket, struct in_addr *addr){
	
	pthread_mutex_t mtx_listening; //mutex of the connection that accept was called on -- the listening connection
	pthread_mutex_t mtx_connecting; //mutex of the newly created connection that goes on to accept connection
	pthread_cond_t cond_connecting; //pthread_cond_t of the newly created connecting connection
	
	tcp_connection_t listening_connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(listening_connection == NULL)
		return -EBADF;

	/* Lock up api on this connection -- BLOCK  -- really we don't want to 
		be able to call this if another api call in process */
	mtx_listening = tcp_connection_get_api_mutex(listening_connection);
	pthread_mutex_lock(&mtx_listening);
	
	/* listening connection must actually be listening */
	if(tcp_connection_get_state(listening_connection) != LISTEN){
		pthread_mutex_unlock(&mtx_listening);
		return -EINVAL; //Socket is not listening for connections, or addrlen is invalid (e.g., is negative).
	}	
	/* calls on the listening_connection to dequeue its triple and node creates new connection with information
	 new socket is the socket assigned to that new connection.  This connection will then go on to finish
	 the three-way handshake to reach ESTABLISHED state */
	/* THIS CALL IS BLOCKING -- because the accept_queue is a bqueue -- call returns when accept_data_t dequeued */
	tcp_connection_t new_connection = tcp_node_connection_accept(tcp_node, listening_connection, addr);
	if(new_connection == NULL){
		pthread_mutex_unlock(&mtx_listening);
		return -1; //TODO -- HANDLE BETTER -- WHICH ERROR CODE? WHAT HAPPENED?
	}
	/* We also need to lock up this new_connection */
	mtx_connecting = tcp_connection_get_api_mutex(new_connection);
	cond_connecting = tcp_connection_get_api_cond(new_connection);
	
	// set state of this new_connection to LISTEN so that we can send it through transition LISTEN_to_SYN_RECEIVED
	tcp_connection_set_state(new_connection, LISTEN);
	
	// have connection transition from LISTEN to SYN_RECEIVED
	if(tcp_connection_state_machine_transition(new_connection, receiveSYN)<0){
		puts("Alex and Neil go debug: tcp_connection_state_machine_transition(new_connection, receiveSYN)) returned negative value in tcp_node_connection_accept");
		exit(1); // CRASH AND BURN
	}
	
	/* Now wait until connection ESTABLISHED 
		when established connection should call tcp_api_accept_help which will signal the accept_cond */
	pthread_cond_wait( &cond_connecting, &mtx_connecting);
	int ret = tcp_connection_get_api_ret(new_connection); // if successful = new_connection->socket_id;

	/* Our connection has been established! 
	TODO:HANDLE bad ret value */
	pthread_mutex_unlock(&mtx_connecting);
	pthread_mutex_unlock(&mtx_listening);
	
	return ret;	
 
}
