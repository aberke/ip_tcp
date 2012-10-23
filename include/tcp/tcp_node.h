// stuct tcp_node owns a table described in WORKING_ON
#ifndef __TCP_NODE_H__ 
#define __TCP_NODE_H__


//// some helpful static globals
#define IP_HEADER_SIZE sizeof(struct ip)
#define UDP_PACKET_MAX_SIZE 1400
#define STDIN fileno(stdin)
#define MTU (UDP_PACKET_MAX_SIZE - IP_HEADER_SIZE)


typedef struct tcp_node* tcp_node_t; 

/*
// following structs defined in ip_node.h
//alex created ip_thread_data struct to pass in following arguments to start:
	struct ip_thread_data{
		ip_node_t ip_node;
		bqueue_t *to_send;
		bqueue_t *to_read;
		bqueue_t *stdin_commands;   // way for tcp_node to pass user input commands to ip_node
	};
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
void tcp_node_destroy(tcp_node_t ip_node);
void tcp_node_print(tcp_node_t tcp_node);

int tcp_node_start_ip_thread(tcp_node_t tcp_node);
int tcp_node_start_stdin_thread(bqueue_t stdin_queue);

void tcp_node_start(tcp_node_t tcp_node);
/* ***************************** */




#endif //__TCP_NODE_H__