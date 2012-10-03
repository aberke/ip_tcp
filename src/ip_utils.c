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
int packet_data_size = ip_check_valid_packet(char* buffer, int bytes_read){
	//make sure packet long enough to be an ip packet
	if(bytes_read < sizeof(struct ip)){
		//packet not large enough
		puts("received packet with length < sizeof(struct ip)");
		return -1;
	}
	u_int header_length;
	u_short ip_len, ip_sum;
	char header[sizeof(struct ip)];
	memcpy(header, buffer, sizeof(struct ip));
	struct ip *ip_header = (struct ip *)header;
	//run checksum
	ip_sum = ip_header->ip_sum;
	if(ip_sum != ip_sum(header, sizeof(struct ip)){
		puts("Packet ip_sum != actually checksum");
		return -1;
	}
	//make sure as long as its supposed to be
	ip_len = ip_header->ip_len;
	if(bytes_read < ip_len){
		//didn't read in entire packet -- error
		puts("bytes read in less than packet length");
		return -1;
	}
	header_length = ip_header->ip_hl;
	return ip_len - ip_hl;
}
// returns destination address of packet
uint32_t dest_addr = ip_get_dest_addr(char* buffer){
	char header[sizeof(struct ip)];
	memcpy(header, buffer, sizeof(struct ip));
	struct ip *ip_header = (struct ip *)header;
	
	struct  in_addr dest_ip;
	uint32_t d_addr;
	ip_dest = ip_header->ip_dest;
	d_addr = ip_dest.s_addr;
	return d_addr;
}
// int is type: RIP vs other  --return -1 if bad packet
// fills packet_unwrapped with data within packet
int type = ip_unwrap_packet(char* buffer, char* packet_unwrapped, int packet_data_size){
	u_int header_length;
	u_char ip_p;           /* protocol */
	char header[sizeof(struct ip)];
	memcpy(header, buffer, sizeof(struct ip));
	struct ip *ip_header = (struct ip *)header;
	header_length = ip_header->ip_hl;
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
