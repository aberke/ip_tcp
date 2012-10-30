// tcp_utils is for helper functions involving wrapping/unwrapping tcp_packets

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "tcp_utils.h"

#define TCP_HEADER_SIZE sizeof(struct tcphdr);


#define NO_OPTIONS_HEADER_LENGTH 5

/* just encapsulates unwrapping the header. the reason this is its own
	function is because we may want to (or at least we should be able to)
	puts some logic in here thats actually checking whether or not the void*
	is indeed a tcp_header. But for now we're just casting it. the length
	parameter is to make these functions all look and feel the same 

	PARAMETERS:
		void* packet 		packet to be unwrapped
		int length 			length of the TCP packet, not any original encapsulation
*/
struct tcphdr* tcp_unwrap_header(void* packet, int length){
	struct tcphdr* header = (struct tcphdr*)packet;
	return header;
}
/*
struct memchunk{  --- define in utils.h
	void* data;
	int length;
};*/ 
/* returns a memchunk with the data in the packet, or NULL if there is no data 

	PARAMETERS:
		void* packet 		packet to be unwrapped
		int length 			length of the TCP packet, not any original encapsulation
*/
memchunk_t tcp_unwrap_data(void* packet, int length){
	struct tcphdr* header = (struct tcphdr*)packet;
	unsigned int data_offset = 4*header->th_off; /* because it's the length in 32-bit words */

	/* if there's no data, just return NULL */
	if(length-data_offset) 
		return NULL;

	memchunk_t payload = memchunk_init(packet+data_offset, length-data_offset);
	return payload;
}

struct tcphdr* tcp_header_init(unsigned short host_port, unsigned short dest_port, uint32_t seq, uint32_t ack){
	struct tcphdr* header = malloc(sizeof(struct tcphdr));
	memset(ip_header, 0, TCP_HEADER_SIZE);
	header->th_sport = htons(host_port);
	header->th_dport = htons(dest_port);
	tcp_set_seq(header, seq);
	tcp_set_ack(header, ack);
	tcp_set_offset(header);
	return header;
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



