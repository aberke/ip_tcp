#include <stdio.h>

#include "routing_table.h"
#include "uthash.h"

#define HOP_COST 1
#define RIP_COMMAND_REQUEST 1
#define RIP_COMMAND_RESPONSE 2

#define MIN(a,b) ((a) < (b) ? (a) : (b))

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

void routing_entry_print(routing_entry_t entry){
	printf("Routing entry: <address:%d> <next-hop:%d> <cost:%d>\n", entry->address, entry->next_hop, entry->cost);
}

void routing_entry_destroy(routing_entry_t* info){
	free(*info);
	*info = NULL;
}

void routing_entry_free(routing_entry_t info){
	free(info);
}

routing_table_t routing_table_init(){
	routing_table_t rt = (struct routing_table*)malloc(sizeof(struct routing_table));
	rt->route_hash = NULL;
	
	return(rt);
}
		
void routing_table_destroy(routing_table_t* rt){
	routing_entry_t info, tmp;

	HASH_ITER(hh, (*rt)->route_hash, info, tmp){
		//routing_entry_print(info);
		HASH_DEL((*rt)->route_hash, info);
		routing_entry_destroy(&info);
	}

	free(*rt);
	*rt = NULL;
} 

/* FUNCTIONALITY */


void routing_table_update_entry(routing_table_t rt, routing_entry_t entry){
	HASH_ADD_INT(rt->route_hash, address, entry); 
}
// next_hop = local_virt_ip -- address of interface that received info
void update_routing_table(routing_table_t rt, forwarding_table_t ft, struct routing_info* info, uint32_t next_hop){
	int i;
	uint32_t addr,cost;
	/* for each entry in info, see if you already have info for that address, or if
 		your current distance is better than the supposed distance (given distance + 1), 
		and if either of these are false, update your talbe with the new info */
	for(i=0;i<info->num_entries;i++){
		/* pull out the address and cost of the current line in the info */
		addr = info->entries[i].address;
		cost = MIN(info->entries[i].cost + HOP_COST, INFINITY);		

		/* now find the hash entry corresponding to that address, and run RIP */
		routing_entry_t entry;
		HASH_FIND_INT(rt->route_hash, &addr, entry); 		
		if(!entry){
			routing_table_update_entry(rt, routing_entry_init(next_hop, cost, addr));			
			forwarding_table_update_entry(ft, addr, next_hop);
		}	
		else if(entry->cost > cost){
			//printf("cost was more.\n");
			HASH_DEL(rt->route_hash, entry);
			routing_entry_free(entry);
			routing_table_update_entry(rt, routing_entry_init(next_hop, cost, addr));
			//TODO: CREATE RIP MESSAGE
			forwarding_table_update_entry(ft, addr, next_hop); 
		}	
		else{
			//printf("Keeping key %d\n", addr);
		}
	}
}

void routing_table_print(routing_table_t rt){
	if(HASH_COUNT(rt->route_hash) == 0)
		printf("[ no routes known ]\n");
	else{
		routing_entry_t info,tmp;
		HASH_ITER(hh, rt->route_hash, info, tmp){
			printf("route entry: <address:%d> <cost:%d> <next-hop:%d>\n", info->address, info->cost, info->next_hop);
		}
	}
}
// Fills out buffer_tofill with routing_info struct -- with all the data in place
// Returns size of routing_info struct it filled 
int routing_table_RIP_response(routing_table_t rt, char* buffer_tofill){
	int num_entries = HASH_COUNT(rt->route_hash);
	if(num_entries*sizeof(struct cost_address) > UDP_PACKET_MAX_SIZE - 20){
		puts("routing_table has more entries than there is space in UDP_PACKET_MAX_SIZE -- cannot send RIP DATA");
		return -1;
	}
	int total_size = sizeof(struct routing_info) + sizeof(struct cost_address)*num_entries;
	// fill in routing_info struct
	struct routing_info* route_info = (struct routing_info *)malloc(total_size);
	route_info->command = htons((uint16_t)RIP_COMMAND_RESPONSE);
	route_info->num_entries = htons((uint16_t)num_entries);
	
	int i = 0;
	routing_entry_t info, tmp;
	HASH_ITER(hh, rt->route_hash, info, tmp){
		route_info->entries[i].cost = info->cost;
		route_info->entries[i].address = info->address;
		i++;
	}
	// copy over route_info to buffer_tofill and free up route_info
	memcpy(buffer_tofill, route_info, total_size);
	free(route_info);
	return total_size;
}
/*	
struct routing_info{
	uint16_t command;
	uint16_t num_entries;
	struct cost_address entries[];
};
struct cost_address{
	uint32_t cost;
	uint32_t address;
};
*/
/* INTERROGATORS */
/* 
Parameters
	routing table
	address

Returns
	- the cost of getting to that address
	- -1 if that address does not have a place in the table */
uint32_t routing_table_get_cost(routing_table_t rt, uint32_t address){
	routing_entry_t entry;
	HASH_FIND_INT(rt->route_hash, &address, entry);
	if(!entry) return -1;
	else return entry->cost;	
}



