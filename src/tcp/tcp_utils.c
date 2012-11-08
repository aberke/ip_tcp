// tcp_utils is for helper functions involving wrapping/unwrapping tcp_packets

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "tcp_utils.h"

#define MAX_BUFFER_LEN 1024 // how big should this be?

#define TCP_HEADER_SIZE sizeof(struct tcphdr);


#define NO_OPTIONS_HEADER_LENGTH 5


// a tcp_connection in the listen state queues this triple on its accept_queue when
// it receives a syn.  Nothing further happens until the user calls accept at which point
// this triple is dequeued and a connection is initiated with this information
// the connection should then set its state to listen and go through the LISTEN_to_SYN_RECEIVED transition
struct accept_queue_data{
	uint32_t local_ip;
	uint32_t remote_ip;
	uint16_t remote_port;
	uint32_t last_seq_received;
};

accept_queue_data_t accept_queue_data_init(uint32_t local_ip,uint32_t remote_ip,uint16_t remote_port,uint32_t last_seq_received){
	 accept_queue_data_t data = (accept_queue_data_t)malloc(sizeof(struct accept_queue_data));
	 data->local_ip = local_ip;
	 data->remote_ip = remote_ip;
	 data->remote_port = remote_port;
	 data->last_seq_received = last_seq_received;
	return data;
}

uint32_t accept_queue_data_get_local_ip(accept_queue_data_t data){
	return data->local_ip;
}

uint32_t accept_queue_data_get_remote_ip(accept_queue_data_t data){
	return data->remote_ip;
}

uint16_t accept_queue_data_get_remote_port(accept_queue_data_t data){
	return data->remote_port;
}

uint32_t accept_queue_data_get_seq(accept_queue_data_t data){
	return data->last_seq_received;
}

void accept_queue_data_destroy(accept_queue_data_t* data){
	free(*data);
	*data = NULL;
}


/* struct that tcp_node handles up with raw data for tcp_connection to handle sending
	tcp_node creates a tcp_connection_to_send_data and queues it to tcp_connection's my_to_send queue 
	tcp_connection reads these structures off its queue */
struct tcp_connection_tosend_data{
	int bytes; // length of data in bytes
	char to_write[MAX_BUFFER_LEN];
};

tcp_connection_tosend_data_t tcp_connection_tosend_data_init(char* to_write, int bytes){

	tcp_connection_tosend_data_t to_send = (tcp_connection_tosend_data_t)malloc(sizeof(struct tcp_connection_tosend_data));
	
	if(bytes > MAX_BUFFER_LEN)
		bytes = MAX_BUFFER_LEN;
	
	to_send->bytes = bytes;
	
	memcpy(to_send->to_write, to_write, bytes);
	
	return to_send;
}
void tcp_connection_tosend_data_destroy(tcp_connection_tosend_data_t to_send){
	free(to_send);
}


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
/*	
	char src[INET_ADDRSTRLEN], dest[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &src_ip, src, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &dest_ip, dest, INET_ADDRSTRLEN);

	printf("calculating checksum: [length %u] [src %s] [dest %s] [proto %d]\n", total_length, src, dest, protocol);
*/

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


