//tcp_utils.h file
#ifndef __TCP_UTILS_H__ 
#define __TCP_UTILS_H__

#include <netinet/tcp.h>
#include "tcp_connection.h"

typedef struct tcp_socket_address{
	uint32_t virt_ip;
	uint16_t virt_port;
} tcp_socket_address_t;

// takes in data and wraps data in header with correct addresses.  
// frees parameter data and mallocs new packet  -- sets data to point to new packet
// returns size of new packet that data points to
int tcp_utils_wrap_packet(void** data, int data_len, tcp_connection_t connection);


//TODO:
void* tcp_utils_add_checksum(void* packet);
int tcp_utils_validate_checksum(void* packet);

#endif