// stuct tcp_node owns a table described in WORKING_ON
#include <inttypes.h>
#include <netinet/ip.h>
#include <time.h>

#include "tcp_node.h"
#include "tcp_utils.h"
#include "tcp_connection.h"
#include "util/utils.h"
#include "util/list.h"
#include "util/parselinks.h"
#include "ip_node.h"

//// select
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

//// hash-map 
#include "uthash.h"


//// some helpful static globals
#define IP_HEADER_SIZE sizeof(struct ip)
#define UDP_PACKET_MAX_SIZE 1400
#define STDIN fileno(stdin)
#define MTU (UDP_PACKET_MAX_SIZE - IP_HEADER_SIZE)


struct tcp_node{
	// owns an ip_node_t that it keeps running to send/receive/listen on its interfaces
	ip_node_t ip_node;
	

};

tcp_node_t tcp_node_init(list_t* links){
	// initalize ip_node -- return FAILURE if ip_node a failure
	ip_node_t ip_node;
	ip_node = ip_node_init(linkedlist);
	if(!ip_node)
		return NULL;
	
	// create to_send and to_read stdin_commands queues that allow tcp_node and ip_node to communicate
	// to_send is loaded with tcp packets by tcp_node that need to be sent by ip_node
	bqueue_t *to_send = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(to_send);
	// to_read is loaded with unwrapped ip_packets by ip_node that tcp_node needs to handle and unwrap as tcp packets
	bqueue_t *to_read = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(to_read);
	// stdin_commands is for the tcp_node to pass user input commands to ip_node that ip_node needs to handle
	bqueue_t *stdin_commands = (bqueue_t*) malloc(sizeof(bqueue_t));   // way for tcp_node to pass user input commands to ip_node
	bqueue_init(stdin_commands);
	
	// set up thread to run ip_node and fill out argument struct ip_thread_data that will be passed into ip_node_start()
	ip_thread_data_t *thread_data = (ip_thread_data_t *) malloc(sizeof(ip_thread_data_t));
	thread_data->ip_node = ip_node;
	thread_data->to_send = to_send;
	thread_data->to_read = to_read;
	thread_data->stdin_commands = stdin_commands;
	
	pthread_t ip_node_thread;
	int status;
	status = pthread_create(&ip_node_thread, NULL, ip_node_start, (void *)thread_data);
    if (status){
         printf("ERROR; return code from pthread_create() is %d\n", status);
         return NULL;
    }
		
	//TODO: START IP_NODE IN A THREAD to communicate with tcp_node
	
	// create kernal table

}

void ip_node_destroy(tcp_node_t* ip_node);
void tcp_node_print(tcp_node_t tcp_node);

void tcp_node_start();
void tcp_node_stop();

