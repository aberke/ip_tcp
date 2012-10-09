#ifndef __ROUTING_TABLE_H__
#define __ROUTING_TABLE_H__

#include <inttypes.h>
#include "forwarding_table.h"

#define INFINITY 16

#define RIP_DATA 200  
#define TEST_DATA 0  
#define IP_PACKET_MAX_SIZE 64000
#define UDP_PACKET_MAX_SIZE 1400
#define IP_HEADER_SIZE 20
#define ROUTING_INFO_HEADER_SIZE 4

struct cost_address{
	uint32_t cost;
	uint32_t address;
};

struct routing_info{
	uint16_t command;
	uint16_t num_entries;
	struct cost_address entries[];
};

typedef struct routing_table* routing_table_t;

routing_table_t routing_table_init();
void routing_table_destroy(routing_table_t* rt);

void update_routing_table(routing_table_t rt, forwarding_table_t ft, struct routing_info* info, uint32_t next_hop_addr);
void routing_table_print(routing_table_t rt);

uint32_t routing_table_get_cost(routing_table_t rt, uint32_t addr);
// Fills out buffer_tofill with routing_info struct -- with all the data in place
// Returns size of routing_info struct it filled 
//int routing_table_RIP_response(routing_table_t rt, char* buffer_tofill);
struct routing_info* routing_table_RIP_response(routing_table_t rt, uint32_t ip, int* size);

#endif // __ROUTING_TABLE_H__
