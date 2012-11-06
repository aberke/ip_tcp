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


