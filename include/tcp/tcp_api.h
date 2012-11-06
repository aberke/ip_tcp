//tcp api
#ifndef __TCP_API_H__ 
#define __TCP_API_H__

#include "tcp_node.h"
#include "tcp_connection.h"
#include "utils.h"


/* Each api call has its associated finishing call which is tcp_connection has its api_function field set to upon api call*/

int tcp_api_connect_finish(tcp_connection_t connection, int ret);
/* connects a socket to an address (active OPEN in the RFC)
returns 0 on success or a negative number on failure */
void tcp_api_connect(tcp_node_t tcp_node, int socket, struct in_addr addr, uint16_t port);


void tcp_api_socket(tcp_node_t tcp_node);
/* binds a socket to a port
always bind to all interfaces - which means addr is unused.
returns 0 on success or negative number on failure */
int tcp_api_bind(tcp_node_t tcp_node, int socket, char* addr, uint16_t port);









#endif //__TCP_API_H__