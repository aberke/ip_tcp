//tcp_utils.h file
#ifndef __TCP_UTILS_H__ 
#define __TCP_UTILS_H__

#include <netinet/tcp.h>

typedef struct tcp_socket_address{
	uint32_t virt_ip;
	uint16_t virt_port;
} tcp_socket_address_t;
//typedef struct tcp_socket_address* tcp_socket_address_t;



#endif