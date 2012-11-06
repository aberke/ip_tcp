//api file

#include "tcp_api.h"



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
		tcp_api_finish(connection, -ENETUNREACH);
	
	tcp_connection_set_remote_ip(connection, addr.s_addr);
	tcp_connection_set_local_ip(connection, local_ip);
	
	int ret = tcp_connection_active_open(connection, tcp_connection_get_remote_ip(connection), port);
}	



