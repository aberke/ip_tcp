//api file

#include "tcp_api.h"
#include "tcp_connection_state_machine_handle.h"



/* Each api call has its associated finishing call which is tcp_connection has its api_function field set to upon api call*/

int tcp_api_connect_finish(tcp_connection_t connection, int ret){
	printf("Connect call on socket %d returned value ", tcp_connection_get_socket(connection));
	if(ret < 0)
		printf("%d\n",ret);
		return -1;
	printf("%d\n", 0);
	return 0;	
}
/* connects a socket to an address (active OPEN in the RFC)
returns 0 on success or a negative number on failure */
void tcp_api_connect(tcp_node_t tcp_node, int socket, struct in_addr addr, uint16_t port){
	
	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)	
		tcp_api_connect_finish(connection,-EBADF); 	 // = The file descriptor is not a valid index in the descriptor table.
	
	/* Lock up api on this connection -- BLOCK */
	tcp_connection_api_lock(connection);
	/* Set api_finish for connection to call once connect should return */
	tcp_connection_set_api_function(connection, (action_f)tcp_api_connect_finish);
	
	/* Make sure connection has a unique port before sending anything so that node can multiplex response */
	if(!tcp_connection_get_local_port(connection))
		tcp_node_assign_port(tcp_node, connection, tcp_node_next_port(tcp_node));
	
	//connection needs to know both its local and remote ip before sending
	uint32_t local_ip = tcp_node_get_local_ip(tcp_node, (addr.s_addr));
	
	if(!local_ip)
		tcp_connection_api_finish(connection, -ENETUNREACH);
	
	tcp_connection_set_remote_ip(connection, addr.s_addr);
	tcp_connection_set_local_ip(connection, local_ip);
	
	tcp_connection_active_open(connection, tcp_connection_get_remote_ip(connection), port);
}
// called by v_socket	
void tcp_api_socket(tcp_node_t tcp_node){
	printf("v_socket returned: ");	
	tcp_connection_t connection = tcp_node_new_connection(tcp_node);
	if(connection == NULL)
		printf("%d\n", -ENFILE); //The system limit on the total number of open files has been reached.
		return;
	int socket = tcp_connection_get_socket(connection);
	printf("%d\n",socket);
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
	tcp_connection_api_lock(connection);
	
	if(tcp_connection_get_local_port(connection))
		return -EINVAL; 	// The socket is already bound to an address.

	tcp_node_assign_port(tcp_node, connection, port);
	
	/* All done so unlock */
	tcp_connection_api_unlock(connection);
	
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

/* called as the tcp_connection_api_function */
int tcp_api_accept_help(pthread_cond_t accept_cond, int ret){
	pthread_cond_signal(&accept_cond);
}	

/* accept a requested connection (behave like unix socket’s accept)
returns new socket handle on success or negative number on failure 
int v accept(int socket, struct in addr *node); */
int tcp_api_accept(tcp_node_t tcp_node, int socket, struct in_addr *addr){
	
	/* Lock up api on this connection -- BLOCK  -- really we don't want to 
		be able to call this if another api call in process */
	tcp_connection_api_lock(connection);
	
	tcp_connection_t listening_connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(listening_connection == NULL)
		return -EBADF;
	
	// calls on the listening_connection to dequeue its triple and node creates new connection with information
	// new socket is the socket assigned to that new connection.  The connection finishes its handshake to get to
	// 	established state
	tcp_connection_t new_connection = tcp_node_connection_accept(tcp_node, listening_connection, addr);
		
	// set state of this new_connection to LISTEN so that we can send it through transition LISTEN_to_SYN_RECEIVED
	tcp_connection_set_state(new_connection, LISTEN);
	
	/* Now set up to block until connection ESTABLISHED */
	pthread_mutex_t accept_mutex;
	pthread_mutex_init(&accept_mutex, NULL);
	pthread_cond_t accept_cond;              /* for a-wakin' up */
	pthread_cond_init( &accept_cond, NULL );
	
	tcp_connection_set_api_function(connection, (action_f)tcp_api_accept_help);
	tcp_connection_set_api_arg(connection, accept_cond);
	
	// have connection transition from LISTEN to SYN_RECEIVED
	if(tcp_connection_state_machine_transition(new_connection, receiveSYN)<0)
		puts("Alex and Neil go debug: tcp_connection_state_machine_transition(new_connection, receiveSYN)) returned negative value in tcp_node_connection_accept");
	
	/* Now wait until connection ESTABLISHED 
		when established connection should call tcp_api_accept_help which will signal the accept_cond */
	pthread_mutex_lock( &accept_mutex);	
	pthread_cond_wait( &accept_cond, &accept_mutex);
	pthread_mutex_unlock(&accept_mutex);
	
	/* Our connection has been established! 
	Or Never got back the ack --- TODO:HANDLE */	
	tcp_connection_api_unlock(connection);
	return tcp_connection_get_socket(new_connection);	
 
}
