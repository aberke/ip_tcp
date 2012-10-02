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
	struct sockaddr remote; //contains the remote IP address and port for sending
	
	uint32_t local_virt_ip;
	uint32_t remote_virt_ip;
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
	// get address info for remote interface
	struct addrinfo hints, *addrinfo;
	memset(&hints,0,sizeof hints);  //make struct empty
	hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    
    if (getaddrinfo(link->remote_phys_host, remote_port, &hints, &addrinfo) != 0){
		perror("Error in getaddrinfo");
		return NULL;
    }
    link_interface_t l_i = (struct link_interface *)malloc(sizeof(struct link_interface));
	
	l_i->sfd = socket_fd;
	l_i->up_down_boolean = 1; //link_interface starts out up
	l_i->remote = *(addrinfo->ai_addr); //contains the remote IP address and port 
	freeaddrinfo(addrinfo); // free the linked-list

	l_i->local_virt_ip = (link->local_virt_ip).s_addr;
	l_i->remote_virt_ip = (link->remote_virt_ip).s_addr;
	
	return l_i;
}
void link_interface_destroy(link_interface_t interface){
	close(interface->sfd);
	free(interface);
}
// sends packet using given link_interface
// data is a packet constructed by node.c-- wraps udp protocol around this ip packet
// data_len = sizeof data
// returns 1 on success, -1 on error/failure
int send_packet(link_interface_t li, void* data, int data_len){
	int socket_fd = li->sfd;
	struct sockaddr remoteaddr = li->remote;
	socklen_t size = sizeof(remoteaddr);
	int sent, bytes_sent;
	sent = sendto(socket_fd, data, data_len, 0, (struct sockaddr*)&remoteaddr, size);
	bytes_sent = sent;
	while(bytes_sent < data_len){
		if(sent < 0){
			// interface needs to go down
			printf("Remote connection %u closed.\n", li->remote_virt_ip);
			bringdown_interface(li);
			return sent;
		}
    	sent = sendto(socket_fd, data+bytes_sent, data_len-bytes_sent, 0, (struct sockaddr*)&remoteaddr, size);
		bytes_sent = bytes_sent + sent;
	}
	return bytes_sent;
}
// helper to read_packet:
// checks that incoming packet from expected remote address
// Returns -1 if given addresses don't match, returns 1 otherwise
int compare_remote_addr(struct sockaddr* a1, struct sockaddr* a2){
	if(a1->sa_family != a2->sa_family){
		printf("Incoming packet from different type of sa_family than expected -- discarding.\n");
		return -1;
	}
	if(a1->sa_family == AF_INET){ 
		//IPV4  -- cast to sockaddr_in to handle IPV4
		struct sockaddr_in *b1 = (struct sockaddr_in *)a1;
		struct sockaddr_in *b2 = (struct sockaddr_in *)a2;
		if((b1->sin_port != b2->sin_port)||(b1->sin_addr.s_addr != b2->sin_addr.s_addr)){
			printf("Incoming ipv4 packet from different address -- discarding.\n");
			return -1;
			}
		}
	else{ 
		//IPV6  -- cast to sockaddr_in6 to handle IPV6
		struct sockaddr_in6 *b1 = (struct sockaddr_in6 *)a1;
		struct sockaddr_in6 *b2 = (struct sockaddr_in6 *)a2;
		if((b1->sin6_port != b2->sin6_port)||(b1->sin6_addr.s6_addr != b2->sin6_addr.s6_addr)){
			printf("Incoming packet from different address -- discarding.\n");
			return -1;
		}
	}
	// addresses match
	return 1;
}
// reads into buffer
// returns ip packet or null
void* read_packet(link_interface_t l_i, char* buffer, int buffer_len){
	int status, sfd;
	sfd = get_sfd(l_i);
	struct sockaddr remote_addr_in, remote_addr;
	remote_addr = l_i->remote;
	socklen_t size = sizeof(remote_addr_in);
	//read in packet
	status = recvfrom(sfd, buffer, buffer_len, 0, (struct sockaddr*)&remote_addr_in, &size);
	//handle packet in buffer
	if(status <= 0){
		//link shut down:
		bringdown_interface(l_i);
		if(status == 0){
			printf("Remote connection %u closed.\n", l_i->remote_virt_ip);
		}
		else{
			printf("Error reading from connection to %u.\n", l_i->remote_virt_ip);
		}
		return NULL;
	}
	// else: check that remote_addr port and host match info -- if not, discard it
	if(compare_remote_addr(&remote_addr_in, &remote_addr) < 0){
		//addresses don't match -- discard packet
		return NULL;
	}
	//deal with packet --return it
	return buffer;
}

// returns sfd	
int get_sfd(link_interface_t l_i){
	return l_i->sfd;
}
// returns local_virt_ip
uint32_t get_local_virt_ip(link_interface_t l_i){
	return l_i->local_virt_ip;
}
//returns remote_virt_ip
uint32_t get_remote_virt_ip(link_interface_t l_i){
	return l_i->remote_virt_ip;
}
// brings down interface
void bringdown_interface(link_interface_t l_i){
	printf("Interface %u down\n", l_i->local_virt_ip);
	l_i->up_down_boolean = 0;
}
// brings interface up
void bringup_interface(link_interface_t l_i){
	printf("Interface %u up\n", l_i->local_virt_ip);
	l_i->up_down_boolean = 1;
}
// queries whether interface up or down
// returns 0 for interface down, 1 for interface up
int interface_up_down(link_interface_t l_i){
	return l_i->up_down_boolean;
}

