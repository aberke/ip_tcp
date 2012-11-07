//tcp api
#ifndef __TCP_API_H__ 
#define __TCP_API_H__

#include "tcp_node.h"
#include "tcp_connection.h"
#include "utils.h"

/* for passing in to the spawned threads */
struct tcp_api_args{
	tcp_node_t node;
	tcp_connection_t connection;
	int socket;
	struct in_addr* addr;
	uint16_t port;
};

typedef struct tcp_api_args* tcp_api_args_t; 
tcp_api_args_t tcp_api_args_init();
void tcp_api_args_destroy(tcp_api_args_t* args);

/* connects a socket to an address (active OPEN in the RFC)
returns 0 on success or a negative number on failure */
int tcp_api_connect(tcp_node_t tcp_node, int socket, struct in_addr* addr, uint16_t port);

int tcp_api_socket(tcp_node_t tcp_node);
/* binds a socket to a port
always bind to all interfaces - which means addr is unused.
returns 0 on success or negative number on failure */
int tcp_api_bind(tcp_node_t tcp_node, int socket, char* addr, uint16_t port);
int tcp_api_listen(tcp_node_t tcp_node, int socket);

/* accept a requested connection (behave like unix socket’s accept)
returns new socket handle on success or negative number on failure 
int v accept(int socket, struct in addr *node); */
int tcp_api_accept(tcp_node_t tcp_node, int socket, struct in_addr *addr);
void* tcp_api_accept_entry(void* args);








#endif //__TCP_API_H__
