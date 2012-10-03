//link_interface.h file
#ifndef __LINK_INTERFACE__
#define __LINK_INTERFACE__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "util/parselinks.h"

#define INTERFACE_SUCCESS 0
#define INTERFACE_ERROR_WRONG_ADDRESS -1
#define INTERFACE_ERROR_FATAL -2

#define IP_PACKET_MAX_SIZE 64000
#define UDP_PACKET_MAX_SIZE 1400

/// Some configuration
#define DISCARD_ON_WRONG_ADDRESS 1

typedef struct link_interface * link_interface_t;

link_interface_t link_interface_create(link_t *link);

void link_interface_destroy(link_interface_t interface);

// helper to link_interface_create
// creates and binds socket and returns socket file descriptor sfd
int link_interface_bind_socket(char* localhost, char* localport, struct addrinfo* local_addrinfo);

// sends packet using given link_interface
// returns 1 on success, -1 on error/failure
int link_interface_send_packet(link_interface_t interface, void* data, int data_len);
/*Read packet */
// helper to read_packet:
// checks that incoming packet from expected remote address
// Returns -1 if given addresses don't match, returns 1 otherwise
int compare_remote_addr(struct sockaddr* a1, struct sockaddr* a2);
// reads into buffer
// returns bytes_read on success, -1 on error or socket closure
int link_interface_read_packet(link_interface_t l_i, char* buffer, int buffer_len);

// returns sfd	
int link_interface_get_sfd(link_interface_t interface);
// returns local_virt_ip
uint32_t link_interface_get_local_virt_ip(link_interface_t l_i);
//returns remote_virt_ip
uint32_t link_interface_get_remote_virt_ip(link_interface_t l_i);
// brings interface down
void link_interface_bringdown(link_interface_t l_i);
// brings interface up
void link_interface_bringup(link_interface_t l_i);
// queries whether interface up or down
// returns 0 for interface down, 1 for interface up
int interface_up_down(link_interface_t l_i);

// prints out info
void link_interface_print(link_interface_t l_i);	

#endif //__LINK_INTERFACE__
