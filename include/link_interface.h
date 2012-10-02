//link_interface.h file
#ifndef __LINK_INTERFACE__
#define __LINK_INTERFACE__

#include "util/parselinks.h"


#define IP_PACKET_MAX_SIZE 64000
#define UDP_PACKET_MAX_SIZE 1400


typedef struct link_interface * link_interface_t;

link_interface_t link_interface_create(link_t *link);

void link_interface_destroy(link_interface_t interface);

// helper to link_interface_create
// creates and binds socket and returns socket file descriptor sfd
int bind_socket(char* localhost, char* localport);

// sends packet using given link_interface
// returns 1 on success, -1 on error/failure
int send_packet(link_interface_t interface, void* data, int data_len);
/*Read packet */
// helper to read_packet:
// checks that incoming packet from expected remote address
// Returns -1 if given addresses don't match, returns 1 otherwise
int compare_remote_addr(struct sockaddr* a1, struct sockaddr* a2);
// reads into buffer
// returns ip packet or null
void* read_packet(link_interface_t l_i, char* buffer, int buffer_len);

// returns sfd	
int get_sfd(link_interface_t interface);
// returns local_virt_ip
uint32_t get_local_virt_ip(link_interface_t l_i);
//returns remote_virt_ip
uint32_t get_remote_virt_ip(link_interface_t l_i);
// brings interface down
void bringdown_interface(link_interface_t l_i);
// brings interface up
void bringup_interface(link_interface_t l_i);
// queries whether interface up or down
// returns 0 for interface down, 1 for interface up
int interface_up_down(link_interface_t l_i);

#endif //__LINK_INTERFACE__