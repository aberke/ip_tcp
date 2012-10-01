#ifndef __FORWARDING_TABLE_H__
#define __FORWARDING_TABLE_H__

#include <inttypes.h>

typedef struct forwarding_table* forwarding_table_t;

forwarding_table_t forwarding_table_init();
void forwarding_table_destroy(forwarding_table_t* ft);

void forwarding_table_update_entry(forwarding_table_t ft, uint32_t address, uint32_t next_hop);

#endif // __FORWARDING_TABLE_H__
