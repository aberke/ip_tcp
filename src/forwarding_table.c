#include <stdio.h>

#include "forwarding_table.h"
#include "uthash.h"

struct forwarding_info{
	uint32_t final_address;
	uint32_t next_hop;
	
	UT_hash_handle hh;
};

typedef struct forwarding_info* forwarding_info_t;

struct forwarding_table {
	struct forwarding_info* entries;
};	

forwarding_info_t forwarding_info_init(uint32_t address, uint32_t next){
	forwarding_info_t info = (forwarding_info_t)malloc(sizeof(struct forwarding_info));
	info->final_address = address;
	info->next_hop = next;
	return info;
}

void forwarding_info_destroy(forwarding_info_t* info){
	free(*info);
	*info = NULL;
}

void forwarding_info_free(forwarding_info_t info){
	free(info);
}

void forwarding_info_print(forwarding_info_t info){
	printf("forwarding info: <final-address:%d> <next-hop:%d>\n", info->final_address, info->next_hop);
} 

forwarding_table_t forwarding_table_init(){
	forwarding_table_t ft = (forwarding_table_t)malloc(sizeof(struct forwarding_table));
	ft->entries = NULL;
	return ft;
}

void forwarding_table_destroy(forwarding_table_t* ft){
	forwarding_info_t info, tmp;

	HASH_ITER(hh,(*ft)->entries,info,tmp){
		//forwarding_info_print(info);
		HASH_DEL((*ft)->entries, info);
		forwarding_info_free(info);
	}

	free(*ft);
	*ft = NULL;
}	

void forwarding_table_update_entry(forwarding_table_t ft, uint32_t addr, uint32_t next){
	forwarding_info_t info;
	HASH_FIND_INT(ft->entries,&addr, info);
	if(info){
		HASH_DEL(ft->entries, info);	
		forwarding_info_destroy(&info);
	}
	info = forwarding_info_init(addr, next);
	HASH_ADD_INT(ft->entries, final_address,info); 
} 

void forwarding_table_print(forwarding_table_t ft){
	forwarding_info_t info, tmp;	
	HASH_ITER(hh, ft->entries, info, tmp){
		forwarding_info_print(info);
	}
} 


// INTERROGATORS 

/* 
Parameters
	forwarding_table, final_address

Returns
	- next-hop that corresponds to the given address
	// next_hop = local_virt_ip 
	- -1 if no such address can be found */
uint32_t forwarding_table_get_next_hop(forwarding_table_t ft, uint32_t final_address){
	forwarding_info_t info;
	HASH_FIND_INT(ft->entries, &final_address, info);
	if(!info) return -1;
	else return info->next_hop;	
}


