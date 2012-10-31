//tcp_utils.h file
#ifndef __TCP_UTILS_H__ 
#define __TCP_UTILS_H__

#include <netinet/tcp.h>
#include "tcp_connection.h"

#include "utils.h"
#include "ip_utils.h" // tcp_packet_data_t and its associated functions defined there

#define TCP_HEADER_MIN_SIZE 20

typedef struct tcp_socket_address{
	uint32_t virt_ip;
	uint16_t virt_port;
} tcp_socket_address_t;


struct tcphdr* tcp_unwrap_header(void* packet, int length);
memchunk_t tcp_unwrap_data(void* packet, int length);

/*
bit shifting fun

x = 0010
x << 1

0100

x >> 1
0001

x = 1001
y = 1100

x & y = 1000

URG ACK PSH RST SYN FIN

x = 0 0 0  0 0 0 

y = 0 0 1  0 0 0 

x |= 1 << 4

1 << 4 
*/


/* these are simple wrappers around extracting data from the
	TCP header. They are macros because it's faster (basically
	an inline function) but we could change them at some point 
	to something different if we wanted to 

	NOTE: converting to host-byte-order is handled!
*/


/****** For Unwrapping *****/

// defined in terms of SIGNIFICANCE!!
#define FIN_BIT 0
#define SYN_BIT 1
#define RST_BIT 2
#define PSH_BIT 3
#define ACK_BIT 4
#define URG_BIT 5

#define tcp_window_size(header) ntohl(((struct tcphdr*)header)->th_win)
#define tcp_ack(header) ntohl(((struct tcphdr*)header)->th_ack)
#define tcp_seqnum(header) ntohl(((struct tcphdr*)header)->th_seq)
#define tcp_dest_port(header) ntohs(((struct tcphdr*)header)->th_dport)
#define tcp_source_port(header) ntohs(((struct tcphdr*)header)->th_dport)

#define tcp_fin_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << FIN_BIT)) > 0) // is fin set? 
#define tcp_syn_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << SYN_BIT)) > 0) // is syn set?
#define tcp_rst_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << RST_BIT)) > 0) // is rst set?
#define tcp_psh_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << PSH_BIT)) > 0) // is psh set?
#define tcp_ack_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << ACK_BIT)) > 0) // is ack set?
#define tcp_urg_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << URG_BIT)) > 0) // is urg set?

/******** For wrapping *****/
#define tcp_set_window_size(header, size) ((((struct tcphdr*)header)->th_win) = htonl(size))
#define tcp_set_ack(header, ack) ((((struct tcphdr*)header)->th_ack) = htonl(ack))
#define tcp_set_seq(header, seq) ((((struct tcphdr*)header)->th_seq) = htonl(seq))
#define tcp_set_offset(header) ((((struct tcphdr*)header)->th_off) = NO_OPTIONS_HEADER_LENGTH)

#define tcp_set_fin_bit(header) ((((struct* tcphdr)header)->th_flags) |= (1 << FIN_BIT)) // set the fin bit to 1
#define tcp_set_syn_bit(header) ((((struct* tcphdr)header)->th_flags) |= (1 << SYN_BIT)) // set the syn bit to 1
#define tcp_set_rst_bit(header) ((((struct* tcphdr)header)->th_flags) |= (1 << RST_BIT)) // set the rst bit to 1
#define tcp_set_psh_bit(header) ((((struct* tcphdr)header)->th_flags) |= (1 << PSH_BIT)) // set the psh bit to 1
#define tcp_set_ack_bit(header) ((((struct* tcphdr)header)->th_flags) |= (1 << ACK_BIT)) // set the ack bit to 1
#define tcp_set_urg_bit(header) ((((struct* tcphdr)header)->th_flags) |= (1 << URG_BIT)) // set the urg bit to 1




// takes in data and wraps data in header with correct addresses.  
// frees parameter data and mallocs new packet  -- sets data to point to new packet
// returns size of new packet that data points to
int tcp_utils_wrap_packet(void** data, int data_len, tcp_connection_t connection);


//TODO:
void* tcp_utils_add_checksum(void* packet);
int tcp_utils_validate_checksum(void* packet);

#endif
