#ifndef __TCP_CONNECTION_H__ 
#define __TCP_CONNECTION_H__
 
typedef struct tcp_connection* tcp_connection_t;  

tcp_connection_t tcp_connection_init(int socket);
void tcp_connection_destroy(tcp_connection_t connection);

uint16_t tcp_connection_get_remote_port(tcp_connection_t connection);
uint16_t tcp_connection_get_local_port(tcp_connection_t connection);

uint32_t tcp_connection_get_remote_ip(tcp_connection_t connection);
uint32_t tcp_connection_get_local_ip(tcp_connection_t connection);

int tcp_connection_get_socket(tcp_connection_t connection);

#endif //__TCP_CONNECTION_H__
