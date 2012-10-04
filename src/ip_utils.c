#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/ip.h>
#include <inttypes.h>

#include "ip_utils.h"
#include "util/ipsum.h"

#define RIP_DATA 200  
#define TEST_DATA 0  


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
	checksum = ip_header->ip_sum;
	if(checksum != ip_sum(header, sizeof(struct ip))){
		puts("Packet ip_sum != actually checksum");
		return -1;
	}
	//make sure as long as its supposed to be
	u_short ip_len;
	ip_len = ip_header->ip_len;
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
// returns destination address of packet
uint32_t ip_get_dest_addr(char* buffer){
	char header[sizeof(struct ip)];
	memcpy(header, buffer, sizeof(struct ip));
	struct ip *ip_header = (struct ip *)header;
	
	struct  in_addr dest_ip;
	uint32_t d_addr;
	dest_ip = ip_header->ip_dst;
	d_addr = dest_ip.s_addr;
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
/*
struct routing_info{
	uint16_t command;
	uint16_t num_entries;
	struct cost_address entries[];
};
*/





