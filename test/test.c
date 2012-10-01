#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "routing_table.h"

#define DEFAULT_COMMAND 0

struct routing_info* fill_routing_info(int num_entries, uint32_t* costs, uint32_t* addrs){
	struct routing_info* info = malloc(sizeof(struct routing_info) + num_entries*sizeof(struct cost_address));
	info->command = DEFAULT_COMMAND;
	info->num_entries = (uint32_t)num_entries;
	
	int i;
	for(i=0;i<num_entries;i++){
		struct cost_address ca;
		ca.cost = costs[i];
		ca.address = addrs[i];
		memcpy(&info->entries[i], &ca, sizeof(struct cost_address));
	}

	return info;
}

void debug_update_routing_table(routing_table_t rt, forwarding_table_t ft, struct routing_info* info, uint32_t next_hop){

	update_routing_table(rt, ft, info, next_hop);

	puts("printing routing table..");
	routing_table_print(rt);
	puts("\nprinting forwarding table..");
	forwarding_table_print(ft);
}

void test_basic_routing(){
	routing_table_t rt = routing_table_init();
	forwarding_table_t ft = forwarding_table_init();


/* FIRST INFO */
	int num_entries = 2;
	uint32_t costs[2] = {5,7};
	uint32_t addrs[2] = {0,1};

	uint32_t next_hop = 3;
	struct routing_info* info = fill_routing_info(num_entries, costs, addrs);

/* SECOND INFO */
	int num_entries2 = 3;
	uint32_t costs2[3] = {3,9,6};
	uint32_t addrs2[3] = {0,1,2};
	
	uint32_t next_hop2 = 4;
	struct routing_info* info2 = fill_routing_info(num_entries2, costs2, addrs2);

	debug_update_routing_table(rt, ft, info, next_hop);
	debug_update_routing_table(rt, ft, info2, next_hop2);
	
	/* CLEAN UP */
	forwarding_table_destroy(&ft);
	routing_table_destroy(&rt);
}


int main(int argc, char** argv){
	test_basic_routing();

	/* RETURN */
	return(0);
}
