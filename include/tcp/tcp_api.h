//tcp api
#ifndef __TCP_API_H__ 
#define __TCP_API_H__

#include "tcp_node.h"
#include "tcp_connection.h"
#include "utils.h"


/* Each api call has its associated finishing call which is tcp_connection has its api_function field set to upon api call*/

int tcp_api_connect_finish(int return_value);
/* connects a socket to an address (active OPEN in the RFC)
returns 0 on success or a negative number on failure */
void tcp_api_connect(tcp_node_t tcp_node, int socket, struct in_addr addr, uint16_t port);










#endif //__TCP_API_H__