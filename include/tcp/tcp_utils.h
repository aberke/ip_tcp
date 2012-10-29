//tcp_utils.h file
#ifndef __TCP_UTILS_H__ 
#define __TCP_UTILS_H__

#include <netinet/tcp.h>
#include "tcp_connection.h"

#include "utils.h"

typedef struct tcp_socket_address{
	uint32_t virt_ip;
	uint16_t virt_port;
} tcp_socket_address_t;


struct tcphdr* tcp_unwrap_header(void* packet, int length);
memchunk_t tcp_unwrap_data(void* packet, int length);

/* these are simple wrappers around extracting data from the
	TCP header. They are macros because it's faster (basically
	an inline function) but we could change them at some point 
	to something different if we wanted to 

	NOTE: converting to host-byte-order is handled!
*/
#define tcp_window_size(header) ((header)->th_win)
#define tcp_seqnum(header) ((header)->th_seq)
#define tcp_dest_port(header) ntohs((header)->th_dport)
#define tcp_source_port(header) ntohs((header)->th_dport)


// takes in data and wraps data in header with correct addresses.  
// frees parameter data and mallocs new packet  -- sets data to point to new packet
// returns size of new packet that data points to
int tcp_utils_wrap_packet(void** data, int data_len, tcp_connection_t connection);


//TODO:
void* tcp_utils_add_checksum(void* packet);
int tcp_utils_validate_checksum(void* packet);

#endif
