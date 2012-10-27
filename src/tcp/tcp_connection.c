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
#include "state_machine.h"
#include "send_window.h"

#define WINDOW_DEFAULT_TIMEOUT 3;


struct tcp_connection{
	int socket_id;	// also serves as index of tcp_connection in tcp_node's tcp_connections array
	tcp_socket_address_t local_addr;
	tcp_socket_address_t remote_addr;
	// owns state machine
	state_machine_t state_machine;

	// owns window for sending and window for receiving
	send_window_t send_window;
	//receive_window_t receive_window;
	
	//
};

tcp_connection_t tcp_connection_init(int socket){
	// init state machine
	state_machine_t state_machine = state_machine_init();
	// init window
	// Neil I need some default args please

	send_window_t send_window = send_window_init(
	
	tcp_connection_t connection = (tcp_connection_t)malloc(sizeof(struct tcp_connection));
	
	connection->socket_id = socket;
	//zero out virtual addresses to start
	connection->local_addr.virt_ip = 0;
	connection->local_addr.virt_port = 0;
	connection->remote_addr.virt_ip = 0;
	connection->remote_addr.virt_port = 0;
	
	connection->state_machine = state_machine;
	connection->send_window = send_window;
	
	return connection;
}

void tcp_connection_destroy(tcp_connection_t connection){
// TODO FILL IN
	// destroy state machine
	state_machine_destroy(&(connection->state_machine));
	//destroy window
	//window_destroy(&(connection->window));

	free(connection);
	connection = NULL;
}

/********** State Changing Functions *************/


int tcp_connection_passive_open(tcp_connection_t connection){

	state_e e = state_machine_get_state(connection->state_machine);
	if(e != CLOSED){
		// not in a valid state to call passive_open
		printf("Calling passiveOPEN on a connection not in the CLOSED state\n");
		return -1;
	}
	state_machine_transition(connection->state_machine, passiveOPEN);
	return 1;
}

/********** End of Sate Changing Functions *******/


uint16_t tcp_connection_get_local_port(tcp_connection_t connection){
	tcp_socket_address_t addr;
	addr = connection->local_addr;	
	uint16_t virt_port = addr.virt_port;
	return virt_port;
}
uint16_t tcp_connection_get_remote_port(tcp_connection_t connection){
	tcp_socket_address_t addr = connection->remote_addr;	
	uint16_t virt_port = addr.virt_port;
	return virt_port;
}
void tcp_connection_set_local_port(tcp_connection_t connection, uint16_t port){
	connection->local_addr.virt_port = port;
}

uint32_t tcp_connection_get_local_ip(tcp_connection_t connection){
	tcp_socket_address_t addr = connection->local_addr;
	uint32_t virt_ip = addr.virt_ip;
	return virt_ip;
}
uint32_t tcp_connection_get_remote_ip(tcp_connection_t connection){
	tcp_socket_address_t addr = connection->remote_addr;
	uint32_t virt_ip = addr.virt_ip;
	return virt_ip;
}
int tcp_connection_get_socket(tcp_connection_t connection){
	return connection->socket_id;
}

void tcp_connection_print_state(tcp_connection_t connection){
	state_machine_print_state(connection->state_machine);	
}

	
	

	
