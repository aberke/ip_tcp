#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/ip.h>
#include <inttypes.h>

#include "ip_utils.h"
#include "util/ipsum.h"



/*KEEP IN MIND:
"We ask you to design and implement an interface that allows an upper layer to register a handler
for a given protocol number. We’ll leave its speciﬁcs up to you."
*/

/*"When IP packets arrive at their destination, if they aren’t RIP packets, you should
simply print them out in a useful way."*/


//write wrap_ip_packet 
//don't have to deal with fragmentation but make sure you don't send more than limit

//Param: buffer read in, number of bytes read in
//Return value: bytes of data within packet on success, -1 on error
int ip_check_valid_packet(char* buffer, int bytes_read){
	//make sure packet long enough to be an ip packet
	if(bytes_read < sizeof(struct ip)){
		//packet not large enough
		printf("received packet with length < sizeof(struct ip)");
		return -1;
	}
	u_short checksum;
	char header[sizeof(struct ip)];
	memcpy(header, buffer, sizeof(struct ip));
	struct ip *ip_header = (struct ip *)header;
	//run checksum
	checksum = ntohs(ip_header->ip_sum);
	if(checksum != ip_sum(header, sizeof(struct ip))){  
		puts("Packet ip_sum != actually checksum");
		return -1;
	}
	//make sure as long as its supposed to be
	u_short ip_len;
	ip_len = ntohs(ip_header->ip_len);
	if(bytes_read < ip_len){
		//didn't read in entire packet -- error
		puts("bytes read in less than packet length");
		return -1;
	}
	u_int header_length;
	header_length = ip_header->ip_hl;
	int data_len = ip_len - header_length;
	return data_len;
}
// decrements packet's TTL.
// returns -1 if packet needs to be thrown out (if TTL == 0 when received)
int ip_decrement_TTL(char* packet){
	u_char  ip_ttl;         /* time to live */
	struct ip *ip_header = (struct ip *)packet;
	ip_ttl = ip_header->ip_ttl;
	if(ip_ttl <= 0){
		return -1;
	}
	ip_header->ip_ttl = ip_ttl - 1;
	return 1;
}
// returns destination address of packet
uint32_t ip_get_dest_addr(char* buffer){
	char header[sizeof(struct ip)];
	memcpy(header, buffer, sizeof(struct ip));
	struct ip *ip_header = (struct ip *)header;
	
	struct  in_addr dest_ip;
	uint32_t d_addr;
	dest_ip = ip_header->ip_dst;
	d_addr = ntohl(dest_ip.s_addr);
	return d_addr;
}
// int is type: RIP vs other  --return -1 if bad packet
// fills packet_unwrapped with data within packet
int ip_unwrap_packet(char* buffer, char* packet_unwrapped, int packet_data_size){
	u_int header_len;
	u_char ip_p;           /* protocol */
	char header[sizeof(struct ip)];
	memcpy(header, buffer, sizeof(struct ip));
	struct ip *ip_header = (struct ip *)header;
	header_len = ip_header->ip_hl;
	ip_p = ip_header->ip_p;
	memcpy(packet_unwrapped, buffer+header_len, packet_data_size);
	if(ip_p == RIP_DATA){
		return RIP_DATA;
	}
	if(ip_p == TEST_DATA){
		return TEST_DATA;
	}
	puts("Received packet of unknown protocol");
	return -1;
}
// fills wraps ip header around data and sends through interface li
int ip_wrap_send_packet(void* data, int data_len, int protocol, struct in_addr ip_src, struct in_addr ip_dst, link_interface_t li){
	//make sure not to send more than UDP_PACKET_MAX_SIZE
	if(data_len > (UDP_PACKET_MAX_SIZE - 20)){
		data_len = UDP_PACKET_MAX_SIZE - 20;
		puts("packet too long -- truncating data");
	}
	// convert addresses to network byte order
	ip_src.s_addr = htonl(ip_src.s_addr);
	ip_dst.s_addr = htonl(ip_dst.s_addr);
	
	// fill in header
	struct ip* ip_header;
	ip_header->ip_v = 4;
	ip_header->ip_hl = 5;
	ip_header->ip_len = htons(data_len + 20); //add header length to packet length
	ip_header->ip_ttl = 15;
	ip_header->ip_p = protocol;
	ip_header->ip_src = ip_src;
	ip_header->ip_dst = ip_dst;
	ip_header->ip_sum = htons(ip_sum((char *)ip_header, sizeof(struct ip)));
	
	char* to_send = (char*) malloc(sizeof(char)*(data_len + 20));
	//copy header and data into to_send
	memcpy(to_send, ip_header, 20);
	memcpy(to_send+20, data, data_len);
	
	// send on link interface
	link_interface_send_packet(li, to_send, data_len+20);
	// free packet	
	free(to_send);
	return 1;
}
// helper to node -- to call for sending an RIP packet across an interface -- calls ip_wrap_send_packet
// does work of filling in ip_src and ip_dst
int ip_wrap_send_packet_RIP(void* data, int data_len, link_interface_t interface){
	// fills structs ip_src and ip_dst to send out to ip_wrap_send_packet
	struct in_addr ip_src, ip_dst;
	ip_src.s_addr = link_interface_get_local_virt_ip(interface);
	ip_dst.s_addr = link_interface_get_remote_virt_ip(interface);
	// wrap and send packet
	ip_wrap_send_packet(data, data_len, RIP_DATA, ip_src, ip_dst, interface);
	return 1;
}
/*
struct routing_info{
	uint16_t command;
	uint16_t num_entries;
	struct cost_address entries[];
};
*/





