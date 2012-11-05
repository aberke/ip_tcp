//tcp_utils.h file
#ifndef __TCP_UTILS_H__ 
#define __TCP_UTILS_H__

#include <netinet/tcp.h>
#include "tcp_connection.h"

#include "utils.h"
#include "ip_utils.h" // tcp_packet_data_t and its associated functions defined there

#define TCP_HEADER_MIN_SIZE 20

#define WINDOW_DEFAULT_TIMEOUT 3.0
#define WINDOW_DEFAULT_SEND_WINDOW_SIZE 100
#define WINDOW_DEFAULT_SEND_SIZE 2000
#define WINDOW_DEFAULT_ISN 0  // don't actually use this

#define ACCEPT_QUEUE_DEFAULT_SIZE 10

#define DEFAULT_TIMEOUT 12.0
#define DEFAULT_WINDOW_SIZE ((uint16_t)100)
#define DEFAULT_CHUNK_SIZE 100
#define RAND_ISN() rand()

/* tcp_connection_tosend_data_t is what is loaded on and off each tcp_connection's my_to_send queue */
typedef struct tcp_connection_tosend_data* tcp_connection_tosend_data_t;

tcp_connection_tosend_data_t tcp_connection_tosend_data_init(char* to_write, int bytes);
void tcp_connection_tosend_data_destroy(tcp_connection_tosend_data_t to_send);


typedef struct tcp_socket_address{
	uint32_t virt_ip;
	uint16_t virt_port;
} tcp_socket_address_t;


struct tcphdr* tcp_unwrap_header(void* packet, int length);
memchunk_t tcp_unwrap_data(void* packet, int length);

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
#define tcp_source_port(header) ntohs(((struct tcphdr*)header)->th_sport)
#define tcp_offset_in_bytes(header) ((((struct tcphdr*)header)->th_off)*4) 
#define tcp_checksum(header) (((struct tcphdr*)header)->th_sum)

#define tcp_fin_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << FIN_BIT)) > 0) // is fin set? 
#define tcp_syn_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << SYN_BIT)) > 0) // is syn set?
#define tcp_rst_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << RST_BIT)) > 0) // is rst set?
#define tcp_psh_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << PSH_BIT)) > 0) // is psh set?  <-- don't need to handle
#define tcp_ack_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << ACK_BIT)) > 0) // is ack set?
#define tcp_urg_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << URG_BIT)) > 0) // is urg set?  <-- don't need to handle

/******** For wrapping *****/
#define tcp_set_window_size(header, size) ((((struct tcphdr*)header)->th_win) = ((uint16_t)htonl(size)))
#define tcp_set_ack(header, ack) ((((struct tcphdr*)header)->th_ack) = ((uint32_t)htonl(ack)))
#define tcp_set_seq(header, seq) ((((struct tcphdr*)header)->th_seq) = ((uint32_t)htonl(seq)))
#define tcp_set_offset(header) ((((struct tcphdr*)header)->th_off) = NO_OPTIONS_HEADER_LENGTH)
#define tcp_set_checksum(header, sum) ((((struct tcphdr*)header)->th_sum) = sum)

#define tcp_set_fin_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << FIN_BIT)) // set the fin bit to 1
#define tcp_set_syn_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << SYN_BIT)) // set the syn bit to 1
#define tcp_set_rst_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << RST_BIT)) // set the rst bit to 1
#define tcp_set_psh_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << PSH_BIT)) // set the psh bit to 1
#define tcp_set_ack_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << ACK_BIT)) // set the ack bit to 1
#define tcp_set_urg_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << URG_BIT)) // set the urg bit to 1

struct tcphdr* tcp_header_init(unsigned short host_port, unsigned short dest_port, int data_size);

// takes in data and wraps data in header with correct addresses.  
// frees parameter data and mallocs new packet  -- sets data to point to new packet
// returns size of new packet that data points to
//int tcp_utils_wrap_packet(void** data, int data_len, tcp_connection_t connection);

// now defined in tcp_connection.c
//int tcp_wrap_packet_send(tcp_connection_t connection, struct tcphdr* header, void* data, int data_len);


uint16_t tcp_utils_calc_checksum(void* packet, uint16_t total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t proto);
void tcp_utils_add_checksum(void* packet, uint16_t  total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t proto);
int tcp_utils_validate_checksum(void* packet, uint16_t total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t proto);

#endif
