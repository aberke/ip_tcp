#include <stdio.h>

#include "routing_table.h"
#include "uthash.h"

#define HOP_COST 1

/* STRUCTS */
struct routing_entry {
	uint32_t cost;
	uint32_t address;
	uint32_t next_hop;

	UT_hash_handle hh;
};

typedef struct routing_entry* routing_entry_t;

struct routing_table {	
	struct routing_entry* route_hash; 
};	

/* CTORS, DTORS */
routing_entry_t routing_entry_init(uint32_t next_hop, 
		uint32_t cost, 
		uint32_t address)
{
	routing_entry_t entry = (routing_entry_t)malloc(sizeof(struct routing_entry));
	entry->next_hop = next_hop;
	entry->cost = cost;
	entry->address = address;
	return entry;
}

void routing_entry_destroy(routing_entry_t* info){
	free(*info);
	*info = NULL;
}

routing_table_t routing_table_init(){
	routing_table_t rt = (struct routing_table*)malloc(sizeof(struct routing_table));
	rt->route_hash = NULL;
	
	return(rt);
}
		
void routing_table_destroy(routing_table_t* rt){
	routing_entry_t info, tmp;

	HASH_ITER(hh, (*rt)->route_hash, info, tmp){
		HASH_DEL((*rt)->route_hash, info);
		routing_entry_destroy(&info);
	}

	free(*rt);
	*rt = NULL;
} 

/* FUNCTIONALITY */

void routing_table_update_entry(routing_table_t rt, routing_entry_t entry){
	HASH_ADD_INT(rt->route_hash,address,entry); 
}

void update_routing_table(routing_table_t rt, forwarding_table_t ft, struct routing_info* info, uint32_t next_hop){
	int i;
	uint32_t address,cost;
	/* for each entry in info, see if you already have info for that address, or if
 		your current distance is better than the supposed distance (given distance + 1), 
		and if either of these are false, update your talbe with the new info */
	for(i=0;i<info->num_entries;i++){
		/* pull out the address and cost of the current line in the info */
		address = info->entries[i].address;
		cost = info->entries[i].cost;		

		/* now find the hash entry corresponding to that address, and run RIP */
		routing_entry_t entry;
		HASH_FIND_INT(rt->route_hash, &address, entry);
		if(!entry || entry->cost > cost + HOP_COST){
			routing_entry_destroy(&entry);
			routing_table_update_entry(rt, routing_entry_init(next_hop, cost, address));
			forwarding_table_update_entry(ft, address, next_hop);
		}	
	}
}

void routing_table_print(routing_table_t rt){
	routing_entry_t info,tmp;
	HASH_ITER(hh, rt->route_hash, info, tmp){
		printf("route entry: <address:%d> <cost:%d> <next-hop:%d>\n", info->address, info->cost, info->next_hop);
	}
}




