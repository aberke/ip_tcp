//main file outline
#include <stdio.h>

#include "util/list.h"
#include "util/parselinks.h"
#include "tcp_node.h"

int main(int argc, char *argv[]){
	// check arguments
	if (argc != 2){
		printf("usage: ./node linkfile.lnx\n");
		return 0;
	}

	// get linked-list of link_t's
	iplist_t* linkedlist = parse_links(argv[1]);
	
	//initiate tcp_node which will initiate ip_node_t which will create factory and use linkedlist
	tcp_node_t tcp_node = tcp_node_init(linkedlist);

	if(!tcp_node){
		puts("unable to init tcp_node");
		return 1;
	}
	
	// seed the random number generator 
	srand(time(NULL));
	
	// start it up!
	tcp_node_start(tcp_node);
	tcp_node_destroy(tcp_node);

	return 0;
}
