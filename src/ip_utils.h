#ifndef __LINK_INTERFACE__
#define __LINK_INTERFACE__

#include "util/list.h"
#include "link_interface.h"


//Param: buffer read in, number of bytes read in
//Return value: bytes of data within packet on success, -1 on error
int packet_data_size = ip_check_valid_packet(buffer, bytes_read);

// returns destination address of packet
uint32 dest_addr = ip_get_dest_addr(buffer);


// int is type: RIP vs other  --return -1 if bad packet
// fills packet_unwrapped with data within packet
int type = ip_unwrap_packet(char* buffer, char* packet_unwrapped, int packet_data_size);


#endif //__LINK_INTERFACE__