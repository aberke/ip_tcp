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
	if(!(length-data_offset)) 
		return NULL;

	memchunk_t payload = memchunk_init(packet+data_offset, length-data_offset);
	return payload;
}

struct tcphdr* tcp_header_init(unsigned short host_port, unsigned short dest_port, int data_size){
	struct tcphdr* header = malloc(sizeof(struct tcphdr) + data_size);
	memset(header, 0, sizeof(struct tcphdr)+data_size);
	header->th_sport = htons(host_port);
	header->th_dport = htons(dest_port);
	tcp_set_offset(header);
	return header;
}

/* MOVED TO TCP_CONNECTION.c !! */
// Combines the header and data into one piece of data and creates the tcp_packet_data and queues it -- frees data
// return -1 on failure, 1 on success

//int tcp_wrap_packet_send(tcp_connection_t connection, struct tcphdr* header, void* data, int data_len){	
//	//TODO: LAST STEP WITH READYING HEADER IS SETTING CHECKSUM -- SET CHECKSUM!
//	
//
//	/* there is a TON of mallocing, memcpying and freeing going on in this function
//		we have to find a way to do this more efficiently */
//
//
//	// concatenate tcp_packet	-- TODO FOR NEIL: YOU HAVE FANCY THEORIES ABOUT EFFICIENT MEMORY ALLOCATION AND COPYING - IDEAS?
//	/* A better way might be to init the header with room for the data, this would
//	 	allow us to just paste it in right here (1 less malloc, 1 less free) */
//	/*char* packet = (char*)malloc(sizeof(char)*(TCP_HEADER_MIN_SIZE+data_len));
//	memcpy(packet, header, TCP_HEADER_MIN_SIZE);
//	memcpy(packet+TCP_HEADER_MIN_SIZE, packet, data_len);*/
//
//	/* data_len had better be the same size as when you called 
//		tcp_header_init()!! */
//	memcpy(header+tcp_offset_in_bytes(header), data, data_len);
//
//	// no longer need data
//	free(data);
//	
//	// add checksum here?
//	//tcp_utils_add_checksum(packet);
//	tcp_utils_add_checksum(header);
//	
//	// get addresses
//	uint32_t local_virt_ip, remote_virt_ip;
//	local_virt_ip = tcp_connection_get_local_ip(connection);
//	remote_virt_ip = tcp_connection_get_remote_ip(connection);
//	
//	// send off to ip_node as a tcp_packet_data_t
//	//tcp_packet_data_t packet_data = tcp_packet_data_init(packet, data_len+TCP_HEADER_MIN_SIZE, local_virt_ip, remote_virt_ip);
//	tcp_packet_data_t packet_data = tcp_packet_data_init((char*)header, data_len+tcp_offset_in_bytes(header), local_virt_ip, remote_virt_ip);
//
//	// no longer need packet
//	//free(packet);
//	free(header);
//	
//	if(tcp_connection_queue_ip_send(connection, packet_data) < 0){
//		//TODO: HANDLE!
//		puts("Something wrong with sending tcp_packet to_send queue--How do we want to handle this??");	
//		free(packet_data);
//		return -1;
//	}
//
//	return 1;
//}

/* requires the packet with the header as 
	well as information for the pseudo-header (see below) */
uint16_t tcp_utils_calc_checksum(void* packet, uint16_t total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t protocol){
	int i;
	uint32_t sum = 0;
	uint16_t word;

	/* calculcate one's complement sum of the data and the tcp header */
	for(i=0;i<total_length/2;i++){
		word = *((uint16_t*)(((char*)packet)+i*2));
		sum = sum+word;
	}

	/* we're doing full 16-bit words, so if the length is
		odd, then we need to pad the last byte with 0's */
	if((total_length&1)==1){
		// last byte
		char last_byte = *(((char*)packet) + (total_length-1));		
		sum += ((uint16_t)(last_byte << 8)) & 0xFF00;
	}

	/* now compute over the pseudo 96 byte header, which looks like this:
					 +--------+--------+--------+--------+
                     |           Source Address          |
                     +--------+--------+--------+--------+
                     |         Destination Address       |
                     +--------+--------+--------+--------+
                     |  zero  |  PTCL  |    TCP Length   |
                     +--------+--------+--------+--------+
	*/
	sum = sum + ((src_ip>>16) & 0x00FF);
	sum = sum + (src_ip & 0x00FF);
	sum = sum + ((dest_ip>>16) & 0x00FF);
	sum = sum + (dest_ip & 0x00FF);
	sum = sum + total_length + protocol;

	uint16_t result;
	result = (sum & 0xFF) + (sum >> 16);

	return ~result;
}

/* calculates the checksum and adds it to the tcphdr */
void tcp_utils_add_checksum(void* packet, uint16_t total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t protocol){
	/* zero out the checksum */
	tcp_set_checksum(packet, 0);

	uint16_t checksum = tcp_utils_calc_checksum(packet, total_length, src_ip, dest_ip, protocol);
	
	tcp_set_checksum(packet, checksum);
}

/* 
returns
	NOTE: when you're giving me the src_ip and the dest_ip, the src is the person who SENT the
			TCP packet, ie the ip of the remote addr 

	1  if correct
	-1 if incorrect
*/
int tcp_utils_validate_checksum(void* packet, uint16_t total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t protocol){
	/* store the original checksum */
	uint16_t checksum = tcp_checksum(packet);

	/* zero out the checksum to calculate it */
	tcp_set_checksum(packet, 0);

	/* get the actual checksum */
	uint16_t actual_checksum = tcp_utils_calc_checksum(packet, total_length, src_ip, dest_ip, protocol);

	/* XOR the actual checksum and the given checksum, 
	   if it's not 0, then they're not the same */
	if(checksum ^ actual_checksum){
		tcp_set_checksum(packet, checksum);
		return -1;
	}
	else
		return 1;
}


// a tcp_connection owns a local and remote tcp_socket_address.  This pair defines the connection
// struct tcp_socket_address{
// 	uint32_t virt_ip;
// 	uint16_t virt_port;
// };


