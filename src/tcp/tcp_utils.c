// tcp_utils is for helper functions involving wrapping/unwrapping tcp_packets

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "tcp_utils.h"

#define TCP_HEADER_SIZE sizeof(struct tcphdr);


// takes in data and wraps data in header with correct addresses.  
// frees parameter data and mallocs new packet  -- sets data to point to new packet
// returns size of new packet that data points to
int tcp_utils_wrap_packet(void** data, int data_len, tcp_connection_t connection){
	struct ip* ip_header = (struct ip*)malloc(IP_HEADER_SIZE);
	memset(ip_header, 0, IP_HEADER_SIZE);
		
	struct tcphdr header;
	char* packet = malloc(sizeof(struct tcphdr) + data_len);
	
	header->th_sport = htons(tcp_connection_get_local_port(connection));
	header->th_dport = htons(tcp_connection_get_remote_port(connection)):
	header->th_off = htons(5);
	
	
}


// a tcp_connection owns a local and remote tcp_socket_address.  This pair defines the connection
// struct tcp_socket_address{
// 	uint32_t virt_ip;
// 	uint16_t virt_port;
// };
// 
//  81 struct tcphdr {
//  82     unsigned short  th_sport;   /* source port */
//  83     unsigned short  th_dport;   /* destination port */
//  84     tcp_seq th_seq;         /* sequence number */
//  85     tcp_seq th_ack;         /* acknowledgement number */
//  86 #if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
//  87     unsigned int    th_x2:4,    /* (unused) */
//  88             th_off:4;   /* data offset */
//  89 #endif
//  90 #if __DARWIN_BYTE_ORDER == __DARWIN_BIG_ENDIAN
//  91     unsigned int    th_off:4,   /* data offset */
//  92             th_x2:4;    /* (unused) */
//  93 #endif
//  94     unsigned char   th_flags;
//  95 #define TH_FIN  0x01
//  96 #define TH_SYN  0x02
//  97 #define TH_RST  0x04
//  98 #define TH_PUSH 0x08
//  99 #define TH_ACK  0x10
// 100 #define TH_URG  0x20
// 101 #define TH_ECE  0x40
// 102 #define TH_CWR  0x80
// 103 #define TH_FLAGS    (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
// 104 
// 105     unsigned short  th_win;     /* window */
// 106     unsigned short  th_sum;     /* checksum */
// 107     unsigned short  th_urp;     /* urgent pointer */
// 108 };



