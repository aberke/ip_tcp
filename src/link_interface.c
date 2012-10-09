

#include "util/list.h"
#include "util/parselinks.h"
#include "link_interface.h"

#define HOST_MAX_LENGTH 256 // RFC 2181


//wrapper around link_t structure parsed from .lnx file
//abstracts udp link layer
struct link_interface{ 
	int id; //necessary for 'interfaces' command for command-line -- also id corresponds to index in ip_node's array of interfaces
	int sfd; // socket file descriptor on which listening/sending
	// ip_node queries link_interface on its up_down_boolean by calling int interface_up_down(link_interface_t l_i);
	//2 for up and hasn't yet been queried, 1 for up and already queried; -2 for down and not yet queried, -1 for down and already queried
	int up_down_boolean;  
	struct sockaddr local;
	struct sockaddr remote; //contains the remote IP address and port for sending
	
	uint32_t local_virt_ip;
	uint32_t remote_virt_ip;
};

// helper to link_interface_create
// creates and binds socket and returns socket file descriptor sfd
int link_interface_bind_socket(char *localhost, char *localport, struct addrinfo* local_addrinfo){
	int sfd;
    if((sfd = socket(local_addrinfo->ai_family, local_addrinfo->ai_socktype, local_addrinfo->ai_protocol))<0){
		perror("Error: Failed to create socket\n");
        return -1;
    }
    if(bind(sfd, local_addrinfo->ai_addr, local_addrinfo->ai_addrlen) < 0){
    	perror("Error: Failed to bind socket");
    	return -1;
    }
    return sfd;
}
link_interface_t link_interface_create(link_t *link, int id){
	int socket_fd;
	// convert ports from link to character strings for link_interface
	char local_port[32];
	sprintf (local_port, "%u", link->local_phys_port);
	char remote_port[32];
	sprintf (remote_port, "%u", link->remote_phys_port);
	
	struct addrinfo hints, *local_addrinfo, *remote_addrinfo;

	// get address info for remote interface
	memset(&hints,0,sizeof hints);  //make struct empty
	hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    // fill local_addrinfo
    if (getaddrinfo(link->local_phys_host, local_port, &hints, &local_addrinfo) != 0){
		perror("Error in getaddrinfo");
		return NULL;
    }  
    // fill  remote_addrinfo
    if (getaddrinfo(link->remote_phys_host, remote_port, &hints, &remote_addrinfo) != 0){
		perror("Error in getaddrinfo");
		freeaddrinfo(local_addrinfo);
		return NULL;
    }
    // create and bind socket_fd	
	if((socket_fd = link_interface_bind_socket(link->local_phys_host, local_port, local_addrinfo)) < 0){
		freeaddrinfo(local_addrinfo);
		freeaddrinfo(remote_addrinfo);
		//failed
		return NULL;
	}
	// create link_interface
    link_interface_t l_i = (struct link_interface *)malloc(sizeof(struct link_interface));
	
	l_i->id = id;
	l_i->sfd = socket_fd;
	l_i->up_down_boolean = 2; //link_interface starts out up and needs to be able to report up to ip_node when queried
	l_i->remote = *(remote_addrinfo->ai_addr); //contains the remote IP address and port 
	l_i->local = *(local_addrinfo->ai_addr);
	freeaddrinfo(local_addrinfo);
	freeaddrinfo(remote_addrinfo); // free the linked-list

	l_i->local_virt_ip = htonl((link->local_virt_ip).s_addr);
	l_i->remote_virt_ip = htonl((link->remote_virt_ip).s_addr);
	
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
int link_interface_send_packet(link_interface_t li, void* data, int data_len){
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
			link_interface_bringdown(li);
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
		//// I commented this out because I think we should handle this further up. 
		//printf("Incoming packet from different type of sa_family than expected -- discarding.\n");
		return INTERFACE_ERROR_WRONG_ADDRESS;
	}
	if(a1->sa_family == AF_INET){ 
		//IPV4  -- cast to sockaddr_in to handle IPV4
		struct sockaddr_in *b1 = (struct sockaddr_in *)a1;
		struct sockaddr_in *b2 = (struct sockaddr_in *)a2;
		if((b1->sin_port != b2->sin_port)||(b1->sin_addr.s_addr != b2->sin_addr.s_addr)){
			char* remote_addr_exp = malloc(sizeof(char)*INET_ADDRSTRLEN);
			char* remote_addr_got = malloc(sizeof(char)*INET_ADDRSTRLEN);
			printf("Expected remote: %s:%d, got: %s:%d\n", 
				inet_ntop(AF_INET, &(b2->sin_addr.s_addr), remote_addr_exp, INET_ADDRSTRLEN),
				ntohs(b2->sin_port),
				inet_ntop(AF_INET, &(b1->sin_addr.s_addr), remote_addr_got, INET_ADDRSTRLEN),			
				ntohs(b1->sin_port));
				
			free(remote_addr_exp);		
			free(remote_addr_got);
			
			return INTERFACE_ERROR_WRONG_ADDRESS;
			}
		}
	else{ 
		//IPV6  -- cast to sockaddr_in6 to handle IPV6
		struct sockaddr_in6 *b1 = (struct sockaddr_in6 *)a1;
		struct sockaddr_in6 *b2 = (struct sockaddr_in6 *)a2;
		if((b1->sin6_port != b2->sin6_port)||(b1->sin6_addr.s6_addr != b2->sin6_addr.s6_addr)){

			//// let's just print it out for some good old casting fun
			char* remote_addr_exp = malloc(sizeof(char)*INET6_ADDRSTRLEN);
			char* remote_addr_got = malloc(sizeof(char)*INET6_ADDRSTRLEN);
			printf("Expected remote: %s:%d, got: %s:%d\n", 
				inet_ntop(AF_INET6, &(b2->sin6_addr.s6_addr), remote_addr_exp, INET6_ADDRSTRLEN),
				ntohs(b2->sin6_port),
				inet_ntop(AF_INET6, &(b1->sin6_addr.s6_addr), remote_addr_got, INET6_ADDRSTRLEN),			
				ntohs(b1->sin6_port));
				
			free(remote_addr_exp);		
			free(remote_addr_got);
			
			return INTERFACE_ERROR_WRONG_ADDRESS;
		}
	}
	// addresses match
	return INTERFACE_SUCCESS;
}

///// ADDED BY NEIL
// I think we should add different error messages for different things. 
// for instance:
//     INTERFACE_ERROR_WRONG_ADDRESS wrong address
//     INTERFACE_ERROR_FATAL if you're shutting down the interface.
//     ... 

// reads into buffer
// returns bytes_read on success, -1 in error
int link_interface_read_packet(link_interface_t l_i, char* buffer, int buffer_len){
	int bytes_read, sfd;
	sfd = link_interface_get_sfd(l_i);
	struct sockaddr remote_addr_in, remote_addr;
	remote_addr = l_i->remote;
	socklen_t size = sizeof(remote_addr_in);
	//read in packet
	bytes_read = recvfrom(sfd, buffer, buffer_len, 0, (struct sockaddr*)&remote_addr_in, &size);
	//handle packet in buffer
	if(bytes_read <= 0){
		//link shut down:
		link_interface_bringdown(l_i);
		if(bytes_read == 0){
			printf("Remote connection %u closed.\n", l_i->remote_virt_ip);
		}
		else{
			printf("Error reading from connection to %u.\n", l_i->remote_virt_ip);
		}
		return INTERFACE_ERROR_FATAL;
	}

	// else: check that remote_addr port and host match info -- if not, discard it
	if(DISCARD_ON_WRONG_ADDRESS &&
		 compare_remote_addr(&remote_addr_in, &remote_addr) == INTERFACE_ERROR_WRONG_ADDRESS){
		//addresses don't match -- discard packet
		return INTERFACE_ERROR_WRONG_ADDRESS;
	}

	//deal with packet --return it
	return bytes_read;
}

// returns sfd	
int link_interface_get_sfd(link_interface_t l_i){
	return l_i->sfd;
}
// returns local_virt_ip
uint32_t link_interface_get_local_virt_ip(link_interface_t l_i){
	return l_i->local_virt_ip;
}
//returns remote_virt_ip
uint32_t link_interface_get_remote_virt_ip(link_interface_t l_i){
	return l_i->remote_virt_ip;
}
// brings down interface -- sets up_down_boolean to -2 if was previously up (since needs to tell ip_node), otherwise doesn't change it
void link_interface_bringdown(link_interface_t l_i){
	printf("Interface %d down\n", l_i->id);
	if(l_i->up_down_boolean > 0){
		l_i->up_down_boolean = -2;
	}
}
// brings interface up -- sets up_down_boolean to 2 if was previously down (since needs to tell ip_node), otherwise doesn't change
void link_interface_bringup(link_interface_t l_i){
	printf("Interface %d up\n", l_i->id);
	if(l_i->up_down_boolean < 0){
		l_i->up_down_boolean = 2;
	}
}
// simply returns link_interface up_down_boolean
int link_interface_up_down(link_interface_t l_i){
	return l_i->up_down_boolean;
}

// queries whether interface up or down
// returns 0 if status hasn't changed since this function was last called
// if status has recently changed: returns -1 if down, returns 1 if up
int link_interface_query_up_down(link_interface_t l_i){
	if (l_i->up_down_boolean == -2){
		l_i->up_down_boolean = -1; // since now going to tell ip_node
		return -1;
	}
	else if (l_i->up_down_boolean == 2){
		l_i->up_down_boolean = 1;
		return 2;
	}
	//else ((l_i->up_down_boolean == -1) || (l_i->up_down_boolean == 1)){
	// status hasn't changed since last queried -- return 0;
	return 0;
}

//// added by neil
void link_interface_print(link_interface_t l_i){
	char local_buffer[INET_ADDRSTRLEN], remote_buffer[INET_ADDRSTRLEN];
	struct in_addr local, remote;
	local.s_addr = l_i->local_virt_ip; 
	remote.s_addr = l_i->remote_virt_ip;
	char* up_down = "true";
	if(l_i->up_down_boolean < 0){
		up_down = "false";
	}

	printf("Interface %d: <up: %s> <socket: %d> %s:%d %s %s:%d %s\n", 
		l_i->id,
		up_down,
		l_i->sfd,
		"localhost",
		ntohs((*(struct sockaddr_in*)(&l_i->local)).sin_port),
		inet_ntop(AF_INET, &local, local_buffer, INET_ADDRSTRLEN),
		"localhost",
		ntohs((*(struct sockaddr_in*)(&l_i->remote)).sin_port),
		inet_ntop(AF_INET, &remote, remote_buffer, INET_ADDRSTRLEN));
}
