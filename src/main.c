//main file outline
#include <stdio.h>

#include "util/list.h"
#include "util/parselinks.h"
#include "ip_node.h"

int main(int argc, char *argv[]){
	// check arguments
	if (argc != 2){
		printf("usage: ./node linkfile.lnx");
		return 0;
	}
	// get linked-list of link_t's
	list_t* linkedlist = parse_links(argv[1]);
	
	//initiate ip_node_t which will create factory and use linkedlist
	ip_node_t ip_node = ip_node_init(linkedlist);

	// This should probably be done in node-destroy
	//free_links(linkedlist);

	ip_node_start(ip_node);
	
	ip_node_destroy(&ip_node);
	return 0;
	}
