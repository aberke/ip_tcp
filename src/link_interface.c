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

#include "util/list.h"
#include "util/parselinks.h"
#include "link_interface.h"

#define HOST_MAX_LENGTH 256 // RFC 2181


//wrapper around link_t structure parsed from .lnx file
//abstracts udp link layer
struct link_interface{ 
	int sfd; // socket file descriptor on which listening/sending
	int up_down_boolean;  //1 for up, 0 for down
	struct sockaddr_in remote; //contains the remote IP address and port for sending
	
	struct in_addr local_virt_ip;
	struct in_addr remote_virt_ip;
};

// helper to link_interface_create
// creates and binds socket and returns socket file descriptor sfd
int bind_socket(char *localhost, char *localport){
	int status, sfd;
	struct addrinfo hints, *addrinfo;
	memset(&hints,0,sizeof hints);  //make struct empty
	hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    
    if ((status = getaddrinfo(localhost, localport, &hints, &addrinfo)) != 0){
		fprintf(stderr, "Error in getaddrinfo: %s\n", gai_strerror(status));
		return -1;
    }
    if((sfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol))<0){
		perror("Error: Failed to create socket\n");
        return -1;
    }
    if(bind(sfd, addrinfo->ai_addr, addrinfo->ai_addrlen) < 0){
    	perror("Error: Failed to bind socket");
    	return -1;
    }
    freeaddrinfo(addrinfo); // free the linked-list
    return sfd;
}
link_interface_t link_interface_create(link_t *link){
	int socket_fd;
	// convert ports from link to character strings for link_interface
	char local_port[32];
	sprintf (local_port, "%u", link->local_phys_port);
	char remote_port[32];
	sprintf (remote_port, "%u", link->remote_phys_port);
	
	// create and bind socket_fd	
	if((socket_fd = bind_socket(link->local_phys_host, local_port)) < 0){
		//failed
		return NULL;
	}
	link_interface_t l_i = (struct link_interface *)malloc(sizeof(struct link_interface));
	
	struct sockaddr_in remoteaddr;	
	bzero(&remoteaddr,sizeof(remoteaddr));
   	remoteaddr.sin_family = AF_INET;
   	remoteaddr.sin_addr.s_addr=inet_addr(link->remote_phys_host);
   	remoteaddr.sin_port=htons(link->remote_phys_port);
	
	l_i->sfd = socket_fd;
	l_i->up_down_boolean = 1; //link_interface starts out up
	l_i->remote = remoteaddr; //contains the remote IP address and port 

	l_i->local_virt_ip = (struct in_addr)link->local_virt_ip;
	l_i->remote_virt_ip = (struct in_addr)link->remote_virt_ip;
	
	return l_i;
}
void link_interface_destroy(link_interface_t interface){
	close(interface->sfd);
	free(interface);
}


// sends packet using given link_interface
// data is a packet constructed by node.c-- wraps udp protocol around this ip packet
// returns 1 on success, -1 on error/failure
int send_packet(link_interface_t interface, void* data){

	return 1;
}

// returns ip packet or null
void* read_packet(link_interface_t l_i){
	int status, sfd;
	sfd = get_sfd(l_i);
	char buffer[IP_PACKET_MAX_SIZE] = {0};
	struct sockaddr_in remote_addr;
	socklen_t size = sizeof(remote_addr);
	
	
	status = recvfrom(sfd, buffer, IP_PACKET_MAX_SIZE, 0, (struct sockaddr*)&remote_addr, &size);
	//if status == 0: link shut down
	// if status < 0: error
	// if status > 0:
		//check that remote_addr port and host match info
		//deal with packet
	
	
	return NULL;
}

// returns sfd	
int get_sfd(link_interface_t l_i){
	return l_i->sfd;
}
// returns local_virt_ip
struct in_addr get_local_virt_ip(link_interface_t l_i){
	return l_i->local_virt_ip;
}
//returns remote_virt_ip
struct in_addr get_remote_virt_ip(link_interface_t l_i){
	return l_i->remote_virt_ip;
}

