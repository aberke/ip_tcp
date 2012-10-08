#ifndef __IP_UTILS_H__ 
#define __IP_UTILS_H__

//Param: buffer read in, number of bytes read in
//Return value: bytes of data within packet on success, -1 on error
int ip_check_valid_packet(char* buffer, int bytes_read);

// returns destination address of packet
uint32_t ip_get_dest_addr(char* buffer);
// decrements packet's TTL.
// returns -1 if packet needs to be thrown out (if TTL == 0 when received)
int ip_decrement_TTL(char* packet);
// Returns type: RIP vs other  --return -1 if bad packet
// fills packet_unwrapped with data within packet
int ip_unwrap_packet(char* buffer, char* packet_unwrapped, int packet_data_size);

// fills packet_wrapped with packet_data and header
int ip_wrap_packet(char* packet_data, char* packet_wrapped, int protocol, struct in_addr ip_src, struct in_addr ip_dst);

#endif //__LINK_INTERFACE__
