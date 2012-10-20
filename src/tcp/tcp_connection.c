/* tcp_connection.c file
	struct for handling each tcp_connection.  
		Has socket_id that the kernal tcp_node can store to refer to.
		maintains own statemachine
		maintains own sliding window protocol
		owns:
			local tcp_socket_address{virt_ip, virt_port}
			remote tcp_socket_address
			
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <inttypes.h>

#include "tcp_connection.h"
#include "tcp_utils.h"

struct tcp_connection{
	int socket_id;
	tcp_socket_address_t local_addr;
	tcp_socket_address_t remote_addr;
	// responsible for state machine?


};
		
/* state machine notes:
	The sequence number of the first data octet in this segment (except
    when SYN is present). If SYN is present the sequence number is the
    initial sequence number (ISN) and the first data octet is ISN+1.
	
	
	

	