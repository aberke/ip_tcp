#include <stdio.h>
#include <time.h>

#include "routing_table.h"
#include "ip_utils.h"
#include "uthash.h"

#define HOP_COST 1
#define RIP_COMMAND_REQUEST 1
#define RIP_COMMAND_RESPONSE 2
#define REFRESHED_TIMEOUT 12

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define LOCAL 0
#define FOREIGN 1

/* STRUCTS */
struct routing_entry {
	uint32_t cost;
	uint32_t address;
	uint32_t next_hop;
	time_t last_refreshed;
	int local;

	UT_hash_handle hh;
};
typedef struct routing_entry* routing_entry_t;


struct routing_table {	
	struct routing_entry* route_hash; 
};	

/* CTORS, DTORS */
routing_entry_t routing_entry_init(uint32_t next_hop, 
		uint32_t cost, 
		uint32_t address, 
		int entry_type)
{
	routing_entry_t entry = (routing_entry_t)malloc(sizeof(struct routing_entry));
	entry->next_hop = next_hop;
	entry->cost = cost;
	entry->address = address;
	entry->local = entry_type;
	time(&entry->last_refreshed);
	return entry;
}

//// static internal functions /////
static void _set_to_infinity(routing_table_t rt, forwarding_table_t ft, routing_entry_t entry);

void routing_entry_print(routing_entry_t entry){
	char address[INET_ADDRSTRLEN], next_hop[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &entry->address, address, INET_ADDRSTRLEN*sizeof(char));
	inet_ntop(AF_INET, &entry->next_hop, next_hop, INET_ADDRSTRLEN*sizeof(char));
	char* isLocal = "";
	if(entry->local == LOCAL){
		isLocal = "LOCAL";
	}
	printf("Routing entry: <address:%s> <next-hop:%s> <cost:%d>  --%s--\n", address, next_hop, ntohs(entry->cost), isLocal);
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
		
		HASH_DEL((*rt)->route_hash, info);
		routing_entry_destroy(&info);
	}

	free(*rt);
	*rt = NULL;
} 

/* FUNCTIONALITY */

static void _set_to_infinity(routing_table_t rt, forwarding_table_t ft, routing_entry_t entry){
	if(entry->cost != htons(INFINITY)){
		entry->cost = htons(INFINITY);
		forwarding_table_delete(ft, entry->address);
	}
}

void routing_table_check_timers(routing_table_t rt, forwarding_table_t ft){
	time_t now;
	time(&now);		

	routing_entry_t entry, tmp;
	HASH_ITER(hh, rt->route_hash, entry, tmp){
		if(!(entry->local == LOCAL)){ 
			if(difftime(now, entry->last_refreshed) > REFRESHED_TIMEOUT)			
				_set_to_infinity(rt, ft, entry);
		}
	}
}

void routing_table_update_entry(routing_table_t rt, routing_entry_t entry){
	HASH_ADD(hh, rt->route_hash, address, sizeof(uint32_t), entry); 
}

// next_hop = local_virt_ip -- address of interface that received info
void update_routing_table(routing_table_t rt, forwarding_table_t ft, struct routing_info* info, uint32_t next_hop, int information_type){
	int i;
	uint32_t addr,cost;

	/* for each entry in info, see if you already have info for that address, or if
 		your current distance is better than the supposed distance (given distance + 1), 
		and if either of these are false, update your talbe with the new info */
	for(i=0;i<ntohs(info->num_entries);i++){

		/* pull out the address and cost of the current line in the info */
		addr = info->entries[i].address;

		if(addr == next_hop && information_type == EXTERNAL_INFORMATION){
			// what do other people know about your own vips that you don't? remember, next_hop
			// is a LOCAL ip
			continue; 
		}

		if(information_type == INTERNAL_INFORMATION)
			cost = info->entries[i].cost;
		else
			cost = htons(MIN(ntohs(info->entries[i].cost) + HOP_COST, INFINITY));

		/* now find the hash entry corresponding to that address, and run RIP */
		routing_entry_t entry;
		HASH_FIND(hh, rt->route_hash, &addr, sizeof(uint32_t), entry); 		

		int type = (information_type == INTERNAL_INFORMATION ? LOCAL : FOREIGN);
		
		if(!entry){
			routing_table_update_entry(rt, routing_entry_init(next_hop, cost, addr, type));
			if(cost != INFINITY){
				forwarding_table_update_entry(ft, addr, next_hop);
			}
		}	
		else if( entry->cost > cost || information_type == INTERNAL_INFORMATION || entry->next_hop==next_hop ){
				
			time(&entry->last_refreshed);
			
			if(cost == htons(INFINITY)){
				//our entry's source is giving us an update on that entry -- iff that node set entry to INFINITY, we do too
				_set_to_infinity(rt, ft, entry);
			}
			else{
				HASH_DEL(rt->route_hash, entry);
				routing_entry_free(entry);
	
				routing_table_update_entry(rt, routing_entry_init(next_hop, cost, addr, type));
				forwarding_table_update_entry(ft, addr, next_hop); 
			}
		}
	}
}

void routing_table_print(routing_table_t rt){
	if(HASH_COUNT(rt->route_hash) == 0)
		printf("[ no routes known ]\n");
	else{
		routing_entry_t info,tmp;
		char address[INET_ADDRSTRLEN], next_hop[INET_ADDRSTRLEN];
		HASH_ITER(hh, rt->route_hash, info, tmp){
			char* isLocal = "";
			if(info->local == LOCAL){
				isLocal = "LOCAL";
			}
			inet_ntop(AF_INET, &info->address, address, sizeof(char)*INET_ADDRSTRLEN);
			inet_ntop(AF_INET, &info->next_hop, next_hop, sizeof(char)*INET_ADDRSTRLEN);
			printf("route entry: <address:%s> <cost:%d> <next-hop:%s>  -%s-\n", address, ntohs(info->cost), next_hop, isLocal);
		}
	}
}
// Fills out buffer_tofill with routing_info struct -- with all the data in place
// Returns size of routing_info struct it filled 
struct routing_info* routing_table_RIP_response(routing_table_t rt, uint32_t to, int* size, int request_type){
	int num_entries = HASH_COUNT(rt->route_hash);
	//printf("num_entries: %d\n", num_entries);
	if(num_entries*sizeof(struct cost_address) > UDP_PACKET_MAX_SIZE - IP_HEADER_SIZE - ROUTING_INFO_HEADER_SIZE){ 
		puts("routing_table has more entries than there is space in UDP_PACKET_MAX_SIZE -- cannot send RIP DATA");
		return NULL;
	}

	int total_size = sizeof(struct routing_info) + sizeof(struct cost_address)*num_entries;
	// fill in routing_info struct
	struct routing_info* route_info = (struct routing_info *)malloc(total_size);
	route_info->command = htons((uint16_t)RIP_COMMAND_RESPONSE);
	route_info->num_entries = htons((uint16_t)num_entries);
	
	//if(request_type == INTERNAL_INFORMATION) puts("Sending internal information...");
	//else puts("Sending external information...");
	
	int i = 0;
	uint32_t cost;
	routing_entry_t info, tmp;
	HASH_ITER(hh, rt->route_hash, info, tmp){
		route_info->entries[i].address = info->address;

		/* split horizon with poison reverse */
		if(to==info->next_hop && ntohs(info->cost) != 0){
			cost = htons(INFINITY);
		}
		else{
			cost = info->cost;
		}
		route_info->entries[i].cost = cost;

		i++;
	}
	
	//// also let the caller know of the size
	*size = total_size;
	return route_info;
}

void routing_table_bring_down(routing_table_t rt, forwarding_table_t ft, uint32_t dead_local_ip){
	routing_entry_t entry,tmp;
	HASH_ITER(hh, rt->route_hash, entry, tmp){
		if(entry->next_hop == dead_local_ip ){
			_set_to_infinity(rt, ft, entry);
		}
	}
}

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
	HASH_FIND(hh, rt->route_hash, &address, sizeof(uint32_t), entry);
	if(!entry) return -1;
	else return entry->cost;	
}



