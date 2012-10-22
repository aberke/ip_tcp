// stuct tcp_node owns a table described in WORKING_ON
#include <inttypes.h>
#include <netinet/ip.h>
#include <time.h>

#include "tcp_utils.h"
#include "tcp_connection.h"
#include "util/utils.h"
#include "util/list.h"
#include "util/parselinks.h"
#include "bqueue.h"
#include "ip_node.h"
#include "tcp_node.h"

//// select
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

//// hash-map 
#include "uthash.h"


struct tcp_node{
	int running; // 0 for not running, 1 for running // mimics ip_node
	ip_node_t ip_node;	// owns an ip_node_t that it keeps running to send/receive/listen on its interfaces
	bqueue_t *to_send;	//--- tcp data for ip to send
	bqueue_t *to_read;	//--- tcp data that ip pushes on to queue for tcp to handle
	bqueue_t *stdin_commands;	//---  way for tcp_node to pass user input commands to ip_node
	// owns statemachine
	// owns window
	// owns table of tcp_connections
};
/* helper function to tcp_node_start -- does the work of starting up ip_node_start() in a thread */
int tcp_node_start_ip(tcp_node_t tcp_node){
	struct ip_thread_data{
		ip_node_t ip_node;
		bqueue_t *to_send;
		bqueue_t *to_read;
		bqueue_t *stdin_commands;   // way for tcp_node to pass user input commands to ip_node
	};
	// fetch and put arguments into ip_thread_data_t
	struct ip_thread_data* ip_data;
	ip_data->ip_node = tcp_node->ip_node;
	ip_data->to_send = tcp_node->to_send;
	ip_data->to_read = tcp_node->to_read;
	ip_data->stdin_commands = tcp_node->stdin_commands;	
		
	pthread_t ip_node_thread;
	int status;
	status = pthread_create(&ip_node_thread, NULL, ip_node_start, (void *)ip_data);
    if (status){
         printf("ERROR; return code from pthread_create() is %d\n", status);
         return 0;
    }
	return 1;
}

tcp_node_t tcp_node_init(iplist_t* links){
	// initalize ip_node -- return FAILURE if ip_node a failure
	ip_node_t ip_node;
	ip_node = ip_node_init(links);
	if(!ip_node)
		return NULL;

	// create tcp_node
	tcp_node_t tcp_node = (tcp_node_t)malloc(sizeof(struct tcp_node));	
				
	// create to_send and to_read stdin_commands queues that allow tcp_node and ip_node to communicate
	// to_send is loaded with tcp packets by tcp_node that need to be sent by ip_node
	bqueue_t *to_send = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(to_send);
	tcp_node->to_send = to_send;
	// to_read is loaded with unwrapped ip_packets by ip_node that tcp_node needs to handle and unwrap as tcp packets
	bqueue_t *to_read = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(to_read);
	tcp_node->to_read = to_read;
	// stdin_commands is for the tcp_node to pass user input commands to ip_node that ip_node needs to handle
	bqueue_t *stdin_commands = (bqueue_t*) malloc(sizeof(bqueue_t));   // way for tcp_node to pass user input commands to ip_node
	bqueue_init(stdin_commands);	
	tcp_node->stdin_commands = stdin_commands;
	
	// init statemachine
	// init window
	

	
	// create kernal table
	
	//// you're still running right? right
	tcp_node->running = 1;
	
	return tcp_node;
}

void tcp_node_destroy(tcp_node_t tcp_node){
	// destroy ip_node
	ip_node_t ip_node = tcp_node->ip_node;
	ip_node_destroy(&ip_node);
	// destroy bqueues
	bqueue_destroy(tcp_node->to_send);
	bqueue_destroy(tcp_node->to_read);
	bqueue_destroy(tcp_node->stdin_commands);
	
	// TODO: REST OF CLEAN UP
	
	//destroy statemachine
	// destroy window

}
void tcp_node_print(tcp_node_t tcp_node);

void tcp_node_start(tcp_node_t tcp_node){
	// start up ip_node
	if(!tcp_node_start_ip(tcp_node)){
		// failed to start thread that runs ip_node_start() -- get out and destroy
		puts("Failed to start ip_node");
		return;
	}

}

