#ifndef __LINK_INTERFACE__
#define __LINK_INTERFACE__

#include "util/list.h"
#include "link_interface.h"


//Param: buffer read in, number of bytes read in
//Return value: bytes of data within packet on success, -1 on error
int ip_check_valid_packet(char* buffer, int bytes_read);

// returns destination address of packet
uint32_t ip_get_dest_addr(char* buffer);


// Returns type: RIP vs other  --return -1 if bad packet
// fills packet_unwrapped with data within packet
int ip_unwrap_packet(char* buffer, char* packet_unwrapped, int packet_data_size);


#endif //__LINK_INTERFACE__