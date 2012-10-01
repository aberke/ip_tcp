//main file outline


#include "util/list.h"
#include "parselinks.h"
#include "node.h"

int main(int argc, char *argv[]){
	// check arguments
	if (argc != 2){
		printf("usage: ./node linkfile.lnx");
		return 0;
	}
	// get linked-list of link_t's
	list_t* linkedlist = parse_links(argv[1]);
	
	//initiate node_t which will create factory and use linkedlist
	node_t node = node_init(linkedlist);
	free_links(linkedlist);
	
	node_start(node);
	
	node_destroy(node);
	return 0;
}