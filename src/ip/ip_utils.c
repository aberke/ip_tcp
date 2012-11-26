#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/ip.h>
#include <inttypes.h>

#include "ip_utils.h"
#include "ipsum.h"
#include "utils.h"

/****** Structs/Functions for tcp_packet **************************/

/* data type that to_send and to_read queues will store (ie queue and dequeue) -- need vip's associated with packet
struct tcp_packet_data{
	uint32_t local_virt_ip;
	uint32_t remote_virt_ip;
	char packet[MTU];
	int packet_size;  //size of packet in bytes
};*/

tcp_packet_data_t tcp_packet_data_init(char* packet_data, int packet_data_size, uint32_t local_virt_ip, uint32_t remote_virt_ip){

	tcp_packet_data_t tcp_packet = (tcp_packet_data_t)malloc(sizeof(struct tcp_packet_data));
	
	tcp_packet->local_virt_ip = local_virt_ip;
	tcp_packet->remote_virt_ip = remote_virt_ip;
	tcp_packet->packet_size = packet_data_size;
	tcp_packet->packet = packet_data;
	
	return tcp_packet;
}

void tcp_packet_data_destroy(tcp_packet_data_t* packet_data){
	free((*packet_data)->packet);
	free(*packet_data);
	*packet_data = NULL;
}

void tcp_packet_print(tcp_packet_data_t packet_data){
	char* to_print = malloc(sizeof(char)*(packet_data->packet_size + 1));
	memcpy(to_print, packet_data->packet, packet_data->packet_size);
	to_print[packet_data->packet_size] = '\0';
	printf("packet: %s\n", to_print);
}


/****** End of Structs/Functions for tcp_packet **************************/



//don't have to deal with fragmentation but make sure you don't send more than limit

//Param: buffer read in, number of bytes read in
//Return value: bytes of data within packet on success, -1 on error
int ip_check_valid_packet(char* buffer, int bytes_read){
	if(bytes_read < IP_HEADER_SIZE){
		//packet not large enough
		printf("received packet with length < sizeof(struct ip)");
		return -1;
	}

	struct ip* ip_header = (struct ip*)buffer;

	u_short got_sum = ip_header->ip_sum;
	ip_header->ip_sum = 0; // !! because we're computing it! 

	u_short expected_sum = ip_sum((char*)ip_header, IP_HEADER_SIZE);

	/* if what you get isn't what they got, then that's not good */
	if( got_sum != expected_sum ){  
		print(("Packet ip_sum != actually checksum"), IP_PRINT);
		return -1;
	}

	ip_header->ip_ttl--; 
	ip_header->ip_sum = ip_sum((char*)ip_header, IP_HEADER_SIZE); // set it back

	u_short ip_len = ntohs(ip_header->ip_len);

	if(bytes_read < ip_len){
		//didn't read in entire packet -- error
		puts("bytes read in less than packet length");
		return -1;
	}
	u_int header_length;
	header_length = ip_header->ip_hl*4;
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

// returns source address of packet
uint32_t ip_get_src_addr(char* buffer){
	struct ip *ip_header = (struct ip *)buffer;
	return ip_header->ip_src.s_addr;
}
// returns destination address of packet
uint32_t ip_get_dest_addr(char* buffer){
	struct ip *ip_header = (struct ip *)buffer;
	return ip_header->ip_dst.s_addr;
}

// int is type: RIP vs other  --return -1 if bad packet
// fills packet_unwrapped with data within packet
int ip_unwrap_packet(char* buffer, char** packet_unwrapped, int packet_data_size){

	u_char ip_p = ((struct ip*)buffer)->ip_p;           /* protocol */
	u_int header_len = ((struct ip*)buffer)->ip_hl*4; //// from wikipedia: header length in bytes = value set in ip_hl x 4
	*packet_unwrapped = malloc(packet_data_size);
	memcpy(*packet_unwrapped, buffer+header_len, packet_data_size);
	
	switch(ip_p){
		case RIP_DATA:
			return RIP_DATA;
		case TEST_DATA:
			return TEST_DATA;
		case TCP_DATA:
			return TCP_DATA;
		default:
			puts("Received packet of unknown protocol");
			return -1;
	}
}

// fills wraps ip header around data and sends through interface li
int ip_wrap_send_packet(void* data, int data_len, int protocol, struct in_addr ip_src, struct in_addr ip_dst, link_interface_t li){
	
	if(protocol == TCP_DATA){	
		uint16_t checksum = tcp_checksum(data);
		print(("ip_wrap_send_packet: checksum: %u", checksum), PACKET_PRINT);
	}
	//make sure not to send more than UDP_PACKET_MAX_SIZE
	if(data_len > (UDP_PACKET_MAX_SIZE - IP_HEADER_SIZE)){
		data_len = UDP_PACKET_MAX_SIZE - IP_HEADER_SIZE;
		puts("packet too long -- truncating data");
	}

	// convert addresses to network byte order
	//ip_src.s_addr = htonl(ip_src.s_addr);
	//ip_dst.s_addr = htonl(ip_dst.s_addr);

	// fill in header
	struct ip* ip_header = (struct ip*)malloc(IP_HEADER_SIZE);
	memset(ip_header, 0, IP_HEADER_SIZE);
	ip_header->ip_v = 4;
	ip_header->ip_hl = 5;
	ip_header->ip_len = htons(data_len + IP_HEADER_SIZE); //add header length to packet length

	ip_header->ip_p   = protocol;
	ip_header->ip_src = ip_src;
	ip_header->ip_dst = ip_dst;
	ip_header->ip_sum = 0;

	ip_header->ip_ttl = 15;

	ip_header->ip_sum = ip_sum((char *)ip_header, IP_HEADER_SIZE);
	
	char* to_send = (char*) malloc(sizeof(char)*(data_len + IP_HEADER_SIZE));
	//copy header and data into to_send
	memcpy(to_send, ip_header, IP_HEADER_SIZE);
	memcpy(to_send+IP_HEADER_SIZE, data, data_len);

	// send on link interface
	link_interface_send_packet(li, to_send, data_len+IP_HEADER_SIZE);
	// free packet	
	free(ip_header);
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


void print_packet(char* packet, int size){
	int i;
	for(i=0;i<size;i++){
		printf(" %d ", (int)packet[i]);
	}
	puts("");
	fflush(stdout);
}

