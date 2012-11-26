// tcp_utils is for helper functions involving wrapping/unwrapping tcp_packets

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>


#include "ipsum.h"
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
	unsigned int data_offset =tcp_offset_in_bytes(header); /* because it's the length in 32-bit words */
	
	/* if there's no data, just return NULL */
	if(!(length-data_offset)) 
		return NULL;

	void* data = malloc(length-data_offset);
	memcpy(data, packet+data_offset, length-data_offset);
	memchunk_t payload = memchunk_init(data, length-data_offset);

	return payload;
}

struct tcphdr* tcp_header_init(int data_size){
	struct tcphdr* header = malloc(sizeof(struct tcphdr) + data_size);
	memset(header, 0, sizeof(struct tcphdr)+data_size);
	tcp_set_offset(header);
	return header;
}

/* requires the packet with the header as 
	well as information for the pseudo-header (see below) */
uint16_t tcp_utils_calc_checksum(void* packet, uint16_t total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t protocol){

	uint32_t sum 	  = 0;
	uint16_t odd_byte = 0;

	uint16_t* pseudo_packet = (uint16_t*)malloc(total_length+12); // 12 is size in bytes of pseudo-header
	memset(pseudo_packet, 0, total_length+12);
	((uint32_t*)pseudo_packet)[0] = src_ip;
	((uint32_t*)pseudo_packet)[1] = dest_ip; //why of 1 and not 2??
	((uint8_t*)pseudo_packet) [9] = (uint8_t)TCP_DATA;
	((uint16_t*)pseudo_packet)[5] = ntohs((uint16_t)total_length);
	
	memcpy(((char*)pseudo_packet)+12, packet, total_length);

	int n = total_length+12;
	uint16_t* pseudo_packet_iterator = pseudo_packet;
	while (n>1) {
		sum += *pseudo_packet_iterator++;
		n -= 2;
	}

	if (n==1) {
		*(uint8_t*)(&odd_byte) = *(uint8_t*)pseudo_packet_iterator;
		sum += odd_byte;
	}

	// free it 
	free(pseudo_packet);

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	uint16_t result;
	result = ~sum;

	return result;
}

/* calculates the checksum and adds it to the tcphdr */
void tcp_utils_add_checksum(void* packet, uint16_t total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t protocol){
	
	//return; //was segfaulting so ALEX commented out until NEIL fixes
	
	/* zero out the checksum */
	tcp_set_checksum(packet, 0);

	/* calculate it */
	uint16_t checksum = tcp_utils_calc_checksum(packet, total_length, src_ip, dest_ip, protocol);

	print(("Setting our checksum: %u", checksum), PACKET_PRINT);
	/* set it back (sets it in network byte order now) */
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
	
	//return 1; // was segfaulting so ALEX commented out until NEIL fixes
	
	/* store the original checksum */
	uint16_t their_checksum = tcp_checksum(packet);
	
	/* zero out the checksum to calculate it */
	tcp_set_checksum(packet, 0);

	/* get the actual checksum */
	uint16_t our_checksum = tcp_utils_calc_checksum(packet, total_length, src_ip, dest_ip, protocol);
	
	printf("their_sum: %u\n", their_checksum);
	printf("our_sum: %u\n", our_checksum);


/* !!!!!!!!!!!!!!!LADFJSLADJSFLASDFJDKLSFJ ASDLFKJAS DLFKA */
	/* XOR the actual checksum and the given checksum, 
	   if it's not 0, then they're not the same */
	if(their_checksum ^ our_checksum){
		tcp_set_checksum(packet, their_checksum);
		return -1; 
	}
	else
		return 1;
}
//alex wrote for debugging: prints packet - see tcp_wrap_packet_send
void view_packet(struct tcphdr* header, void* data, int data_length){
	print(("[packet]:"), PACKET_PRINT);
	print(("\tsource_port: %u,  dest_port: %u", tcp_source_port(header), tcp_dest_port(header)), PACKET_PRINT);
	print(("\ttcp_seq: %u,  tcp_ack: %u\n", tcp_seqnum(header),tcp_ack(header)), PACKET_PRINT);
	print(("\toffset in bytes: %i\n", tcp_offset_in_bytes(header)), PACKET_PRINT);
	print(("\tsyn_bit:%i, fin_bit:%i, ack_bit:%i, rst_bit:%i, psh_bit:%i, urg_bit:%i", 
		tcp_syn_bit(header), tcp_fin_bit(header), tcp_ack_bit(header), tcp_rst_bit(header), tcp_psh_bit(header), tcp_urg_bit(header)), PACKET_PRINT);
	print(("\twindow_size: %u", tcp_window_size(header)), PACKET_PRINT);

	char buffer[data_length+1];
	memcpy(buffer, data, data_length);
	buffer[data_length] = '\0';

	print(("\tdata: %s", buffer), PACKET_PRINT);
}

// a tcp_connection owns a local and remote tcp_socket_address.  This pair defines the connection
// struct tcp_socket_address{
// 	uint32_t virt_ip;
// 	uint16_t virt_port;
// };


