//link_interface.h file


#ifndef __LINK_INTERFACE__
#define __LINK_INTERFACE__

// sends packet using given link_interface
// returns 1 on success, -1 on error/failure
int send_packet(link_interface_t interface, void* data);

// returns ip packet
void* read_packet(link_interface_t);

#endif //__LINK_INTERFACE__