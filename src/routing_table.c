#include <stdio.h>

#include "routing_table.h"
#include "ip_utils.h"
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
	char address[INET_ADDRSTRLEN], next_hop[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &entry->address, address, INET_ADDRSTRLEN*sizeof(char));
	inet_ntop(AF_INET, &entry->next_hop, next_hop, INET_ADDRSTRLEN*sizeof(char));
	printf("Routing entry: <address:%s> <next-hop:%s> <cost:%d>\n", address, next_hop, ntohs(entry->cost));
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

		char address[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr, address, INET_ADDRSTRLEN*sizeof(char));
		//printf("address: %s, cost: %d, next-hop o -->", address, ntohs(cost));

		/* now find the hash entry corresponding to that address, and run RIP */
		routing_entry_t entry;
		HASH_FIND(hh, rt->route_hash, &addr, sizeof(uint32_t), entry); 		

		if(!entry){
			//puts("Entry didn't already exist. Adding...");
			routing_table_update_entry(rt, routing_entry_init(next_hop, cost, addr));
			forwarding_table_update_entry(ft, addr, next_hop);
		}	

		else if( entry->cost > cost || information_type == INTERNAL_INFORMATION || entry->next_hop==next_hop ){
			char nh_address[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &entry->next_hop, nh_address, INET_ADDRSTRLEN);
			//printf("entry->next-hop: %s", nh_address);

			//puts("Entry->cost > cost || internal info || this is the next hop of the path");
			HASH_DEL(rt->route_hash, entry);
			routing_entry_free(entry);
			routing_table_update_entry(rt, routing_entry_init(next_hop, cost, addr));
			forwarding_table_update_entry(ft, addr, next_hop); 
		}	
		else{
			char nh_address[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &entry->next_hop, nh_address, INET_ADDRSTRLEN);
			//printf("entry->next-hop: %s, discarding\n", nh_address);
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
			inet_ntop(AF_INET, &info->address, address, sizeof(char)*INET_ADDRSTRLEN);
			inet_ntop(AF_INET, &info->next_hop, next_hop, sizeof(char)*INET_ADDRSTRLEN);
			printf("route entry: <address:%s> <cost:%d> <next-hop:%s>\n", address, ntohs(info->cost), next_hop);
		}
	}
}
// Fills out buffer_tofill with routing_info struct -- with all the data in place
// Returns size of routing_info struct it filled 
/*int routing_table_RIP_response(routing_table_t rt, char* buffer_tofill){
	int num_entries = HASH_COUNT(rt->route_hash);
	if(num_entries*sizeof(struct cost_address) > UDP_PACKET_MAX_SIZE - 20){ // why -20? what about the command/num_entries
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
}*/


// Fills out buffer_tofill with routing_info struct -- with all the data in place
// Returns size of routing_info struct it filled 
struct routing_info* routing_table_RIP_response(routing_table_t rt, uint32_t to, int* size, int request_type){
	int num_entries = HASH_COUNT(rt->route_hash);
	//printf("num_entries: %d\n", num_entries);
	if(num_entries*sizeof(struct cost_address) > UDP_PACKET_MAX_SIZE - IP_HEADER_SIZE - ROUTING_INFO_HEADER_SIZE){ // why -20? what about the command/num_entries
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
		if( to==info->next_hop && ntohs(info->cost) != 0){
			cost = htons(INFINITY);
		}
		else{
			cost = info->cost;
		}
	
		char address[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &info->address, address, INET_ADDRSTRLEN);
		//printf("address: %s, cost: %d\n", address, ntohs(cost));	
	
		route_info->entries[i].cost = cost;

		i++;
	}
	
	//// also let the caller know of the size
	*size = total_size;
	return route_info;
}

void routing_table_bring_down(routing_table_t rt, uint32_t dead_local_ip){
	puts("bringing down");
	routing_entry_t entry,tmp;
	HASH_ITER(hh, rt->route_hash, entry, tmp){
		puts("down");
		if( entry->address != dead_local_ip && entry->next_hop == dead_local_ip ){
			entry->cost = htons(INFINITY);
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



