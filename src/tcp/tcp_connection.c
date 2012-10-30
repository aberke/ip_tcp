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
#include "recv_window.h"
#include "queue.h"


#define WINDOW_DEFAULT_TIMEOUT 3.0
#define WINDOW_DEFAULT_SEND_WINDOW_SIZE 100
#define WINDOW_DEFAULT_SEND_SIZE 2000
#define WINDOW_DEFAULT_ISN 0  // don't actually use this

#define ACCEPT_QUEUE_DEFAULT_SIZE 10

#define DEFAULT_TIMEOUT 12.0
#define DEFAULT_WINDOW_SIZE 1000
#define DEFAULT_CHUNK_SIZE 100
#define RAND_ISN rand()


struct tcp_connection{
	int socket_id;	// also serves as index of tcp_connection in tcp_node's tcp_connections array
	tcp_socket_address_t local_addr;
	tcp_socket_address_t remote_addr;
	// owns state machine
	state_machine_t state_machine;

	// owns window for sending and window for receiving
	send_window_t send_window;
	recv_window_t receive_window;
	
	// owns accept queue to queue syns when listening
	queue_t accept_queue;
};

tcp_connection_t tcp_connection_init(int socket){
	// init state machine
	state_machine_t state_machine = state_machine_init();

	// init windows
	send_window_t send_window = send_window_init(WINDOW_DEFAULT_TIMEOUT, WINDOW_DEFAULT_SEND_WINDOW_SIZE, DEFAULT_WINDOW_SIZE, RAND_ISN);
	recv_window_t receive_window = recv_window_init(DEFAULT_WINDOW_SIZE, RAND_ISN);
	
	queue_t accept_queue = queue_init();
	queue_set_size(accept_queue, ACCEPT_QUEUE_DEFAULT_SIZE);
	
	tcp_connection_t connection = (tcp_connection_t)malloc(sizeof(struct tcp_connection));
	
	connection->socket_id = socket;
	//zero out virtual addresses to start
	connection->local_addr.virt_ip = 0;
	connection->local_addr.virt_port = 0;
	connection->remote_addr.virt_ip = 0;
	connection->remote_addr.virt_port = 0;
	
	connection->state_machine = state_machine;
	connection->send_window = send_window;
	connection->receive_window = receive_window;	
	connection->accept_queue = accept_queue;
	
	state_machine_set_argument(state_machine, connection);		

	return connection;
}

void tcp_connection_destroy(tcp_connection_t connection){

	// destroy windows
	send_window_destroy(&(connection->send_window));
	recv_window_destroy(&(connection->receive_window));
	
	// destroy state machine
	state_machine_destroy(&(connection->state_machine));

	free(connection);
	connection = NULL;
}

/*
tcp_connection_handle_packet
	will get called from tcp_node's _handle_packet function with the 
	connection as well as the tcp_packet_data_t. It is this connections
	job to handle the packet, as well as send back response information
	as needed. All meta-information (SYN, FIN, etc.) will probably be 
	passed in to the state-machine, which will take appropriate action
	with state changes. Therefore, the logic of this function should 
	mostly be concerned with passing off the data to the window/validating
	the correctness of the received packet (that it makes sense) 
*/
void tcp_connection_handle_packet(tcp_connection_t connection, tcp_packet_data_t packet){
	
	/* pull out the ack and pass it to the send window */
	uint16_t ack = tcp_ack(packet->packet);
	send_window_ack(connection->send_window, ack);

	/* get the data */
	memchunk_t data = tcp_unwrap_data(packet->packet, packet->packet_size);
	if(data){
//		recv_window_push(connection->recv_window, 
	}

	printf("connection on port %d handling packet\n", (connection->local_addr).virt_port);
	tcp_packet_data_destroy(&packet);
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

/* in the same vein, these are the functions that will be called
	during transitions between states, handled by the state machine */

/* 
tcp_connection_transition_passive_open
	will be called when the connection is transitioning from CLOSED with a passiveOPEN
	transition
*/
void tcp_connection_transition_passive_open(tcp_connection_t connection){
	
}
	

/********** End of State Changing Functions *******/


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

/* FOR TESTING */
void tcp_connection_print(tcp_connection_t connection){
	puts( "IT WORKS!!" );
}
	
void tcp_connection_set_remote(tcp_connection_t connection, uint32_t remote, uint16_t port){
	connection->remote_addr.virt_ip = remote;
	connection->remote_addr.virt_port = port;
}

	
