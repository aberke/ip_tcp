// stuct tcp_node owns a table described in WORKING_ON
#ifndef __TCP_NODE_H__ 
#define __TCP_NODE_H__

#include "list.h"

typedef struct tcp_node* tcp_node_t; 

/*
//alex created ip_thread_data struct to pass in following arguments to start:
typedef struct ip_thread_data{
	ip_node_t ip_node;
	bqueue_t to_send;
	bqueue_t to_read;
	bqueue_t stdin_commands;   // way for tcp_node to pass user input commands to ip_node
} ip_thread_data_t;
// data type that to_send and to_read queues will store (ie queue and dequeue) -- need vip's associated with packet
typedef struct tcp_packet_data{
	uint32_t local_virt_ip;
	uint32_t remote_virt_ip;
	char packet[MTU];
	int packet_size;  //size of packet in bytes
} tcp_packet_data_t;
*/

/* Notice below outward facing commands mimic ip_node -- I think keeping to one pattern will help us stay organized */
tcp_node_t tcp_node_init(iplist_t* links);
void ip_node_destroy(tcp_node_t* ip_node);
void tcp_node_print(tcp_node_t tcp_node);

void tcp_node_start();
void tcp_node_stop();
/* ***************************** */





#endif //__TCP_NODE_H__