#ifndef __TCP_CONNECTION_H__ 
#define __TCP_CONNECTION_H__

#include <inttypes.h>
 
typedef struct tcp_connection* tcp_connection_t;  

tcp_connection_t tcp_connection_init(int socket);
void tcp_connection_destroy(tcp_connection_t* connection);

uint16_t tcp_connection_get_remote_port(tcp_connection_t connection);
uint16_t tcp_connection_get_local_port(tcp_connection_t connection);

void tcp_connection_set_local_port(tcp_connection_t connection, uint16_t port);

uint32_t tcp_connection_get_remote_ip(tcp_connection_t connection);
uint32_t tcp_connection_get_local_ip(tcp_connection_t connection);

int tcp_connection_get_socket(tcp_connection_t connection);

void tcp_connection_print_state(tcp_connection_t connection);


/********** State Changing Functions *************/


int tcp_connection_passive_open(tcp_connection_t connection);

/********** End of Sate Changing Functions *******/

/* for testing */
void tcp_connection_print(tcp_connection_t connection);
void tcp_connection_set_remote(tcp_connection_t connection, uint32_t remote_ip, uint16_t remote_port);

#endif //__TCP_CONNECTION_H__
