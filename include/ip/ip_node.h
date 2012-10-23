#ifndef __IP_NODE_H__
#define __IP_NODE_H__

#include "list.h"
#include "ip_utils.h"
#include "bqueue.h"

/*#define RIP_DATA 200  
#define TEST_DATA 0  
#define IP_PACKET_MAX_SIZE 64000
#define UDP_PACKET_MAX_SIZE 1400*/

//// some helpful static globals
#define STDIN fileno(stdin)
#define SELECT_TIMEOUT 1
#define PTHREAD_COND_TIMEOUT_NSEC 5000000
#define PTHREAD_COND_TIMEOUT_SEC 0

typedef struct ip_node* ip_node_t; 

/*alex created ip_thread_data struct to pass in following arguments to start: */
struct ip_thread_data{
	ip_node_t ip_node;
	bqueue_t *to_send;
	bqueue_t *to_read;
	bqueue_t *stdin_commands;   // way for tcp_node to pass user input commands to ip_node
};
typedef struct ip_thread_data* ip_thread_data_t;

// data type that to_send and to_read queues will store (ie queue and dequeue) -- need vip's associated with packet
typedef struct tcp_packet_data{
	uint32_t local_virt_ip;
	uint32_t remote_virt_ip;
	char packet[MTU];
	int packet_size;  //size of packet in bytes
} tcp_packet_data_t;



ip_node_t ip_node_init(iplist_t* links);
void ip_node_destroy(ip_node_t* ip_node);
void ip_node_print(ip_node_t ip_node);

void *ip_link_interface_thread_run(void *ipdata);
void *ip_send_thread_run(void *ip_data);
void *ip_command_thread_run(void *ip_data);

// returns 1 if true, 0 if false
int ip_node_running(ip_node_t ip_node);


#endif // __IP_NODE_H__
