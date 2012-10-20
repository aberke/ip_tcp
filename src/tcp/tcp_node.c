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
#define STDIN fileno(stdin)


struct tcp_node{
	// owns an ip_node_t that it keeps running to send/receive/listen on its interfaces
	ip_node_t ip_node;
	

};

tcp_node_t tcp_node_init(list_t* links){
	// initalize ip_node -- return FAILURE if ip_node a failure
	ip_node_t ip_node = ip_node_init(linkedlist);
	if(!ip_node)
		return NULL;
	//TODO: START IP_NODE IN A THREAD to communicate with tcp_node
	
	// create kernal table

}

void ip_node_destroy(tcp_node_t* ip_node);
void tcp_node_print(tcp_node_t tcp_node);

void tcp_node_start();
void tcp_node_stop();

