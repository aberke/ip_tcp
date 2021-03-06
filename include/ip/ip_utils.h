#ifndef __IP_UTILS_H__ 
#define __IP_UTILS_H__

#include "link_interface.h"
#include <netinet/ip.h>

#define RIP_DATA 200  
#define TEST_DATA 0  
#define TCP_DATA 6
#define IP_PACKET_MAX_SIZE 64000
#define IP_HEADER_SIZE sizeof(struct ip)
#define UDP_PACKET_MAX_SIZE 1400
#define ROUTING_INFO_HEADER_SIZE 4
#define MTU (UDP_PACKET_MAX_SIZE - IP_HEADER_SIZE)


#define PTHREAD_COND_TIMEOUT_NSEC 5000000
#define PTHREAD_COND_TIMEOUT_SEC 1 //WAY TOO LONG RIGHT??

/****** Structs/Functions for tcp_packet **************************/

// data type that to_send and to_read queues will store (ie queue and dequeue) -- need vip's associated with packet
struct tcp_packet_data{
	uint32_t local_virt_ip;
	uint32_t remote_virt_ip;
	char* packet;//char packet[MTU];
	int packet_size;  //size of packet in bytes
};

typedef struct tcp_packet_data* tcp_packet_data_t;


tcp_packet_data_t tcp_packet_data_init(char* packet_data, int packet_data_size, uint32_t local_virt_ip, uint32_t remote_virt_ip);
void tcp_packet_data_destroy(tcp_packet_data_t* packet_data);
void tcp_packet_print(tcp_packet_data_t packet);

/****** End of Structs/Functions for tcp_packet **************************/


//Param: buffer read in, number of bytes read in
//Return value: bytes of data within packet on success, -1 on error
int ip_check_valid_packet(char* buffer, int bytes_read);

// returns destination address of packet
uint32_t ip_get_dest_addr(char* buffer);
// returns source address of packet
uint32_t ip_get_src_addr(char* buffer);
// decrements packet's TTL.
// returns -1 if packet needs to be thrown out (if TTL == 0 when received)
int ip_decrement_TTL(char* packet);
// Returns type: RIP vs other  --return -1 if bad packet
// fills packet_unwrapped with data within packet
int ip_unwrap_packet(char* buffer, char** packet_unwrapped, int packet_data_size);

// wraps ip header around data and sends through link_interface li to destination
int ip_wrap_send_packet(void* data, int data_len, int protocol, struct in_addr ip_src, struct in_addr ip_dst, link_interface_t li);
// helper to node -- to call for sending an RIP packet across an interface -- calls ip_wrap_send_packet
// does work of filling in ip_src and ip_dst
int ip_wrap_send_packet_RIP(void* data, int data_len, link_interface_t interface);

void print_packet(char* data, int data_len);

#endif //__IP_UTILS__
