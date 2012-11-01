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
	// if user already in accept state, connections handled right away
	///queues connections when user in listen state and not yet accept state
	queue_t accept_queue;  
	
	// needs reference to the to_send queue in order to queue its packets
	bqueue_t *to_send;	//--- tcp data for ip to send
};

tcp_connection_t tcp_connection_init(int socket, bqueue_t *tosend){
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
	connection->to_send = tosend;
	
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

	/* RFC 793: 
		Although these examples do not show connection synchronization using data
		-carrying segments, this is perfectly legitimate, so long as the receiving TCP
  		doesn't deliver the data to the user until it is clear the data is valid
		

		so no matter what, we need to be pushing the data to the receiving window, 
		and we simply shouldn't call get_next until we're in the established state */

	void* tcp_packet_itself = packet->packet;

	/* check if there's any data, and if there is push it to the window,
		but what does the seqnum even mean if the ACKs haven't been synchronized? */
	memchunk_t data = tcp_unwrap_data(tcp_packet_itself);
	if(data){ 
		uint32_t seqnum = tcp_seqnum(tcp_packet_itself);	
		recv_window_receive(connection->receive_window, data->data, data->length, seqnum);
	}	

	/* now check the bits */
	if(tcp_syn_bit(tcp_packet_itself) && tcp_ack_bit(tcp_packet_itself)){
		if(_validate_ack(tcp_connection, tcp_ack(tcp_packet_itself)) < 0){
			/* then you sent a syn with a seqnum that wasn't faithfully returned. 
				what should we do? for now, let's discard */
			puts("Received invalid ack with SYN/ACK. Discarding.");
			return;
		}

		/* inform you're window of the starting sequence number that your peer has chosen */
		recv_window_set_ISN(tcp_connection->receive_window, tcp_seqnum(tcp_packet_itself));

		/* then transition with the state machine */
		state_machine_transition(connection->state_machine, receiveSYN_ACK);

		/* should we just return? should we check if other bits are set? */
		return;
	}
		
	else if(tcp_syn_bit(tcp_packet_itself)){
		state_machine_transition(connection->state_machine, receiveSYN);
	}
	
	else if(tcp_syn_bit(tcp_packet_itself)){
		state_machine_transition(connection->state_machine, receiveACK);
	}

	/*	
	if(state){
	
		case CLOSED: 
			puts("CLOSED packet received a message. Discarding...\n");
			return;
		
		default:
			return;
	}
	*/

	/* pull out the ack and pass it to the send window */
	uint16_t ack = tcp_ack(packet->packet);
	send_window_ack(connection->send_window, ack);

	/* get the data */
	memchunk_t data = tcp_unwrap_data(packet->packet, packet->packet_size);
	if(data){
//		recv_window_push(connection->recv_window, 
	}

	printf("connection on port %d handling packet\n", (connection->local_addr).virt_port);
	tcp_packet_data_destroy(packet);
}

/********** State Changing Functions *************/

// TODO: RETURN TO ALL THESE FUNCTIONS TO DEAL WITH RETURNING CURRENT ERRORS AFTER WE BETTER UNDERSTAND OUR OWN STATEMACHINE


int tcp_connection_passive_open(tcp_connection_t connection){
	return state_machine_transition(connection->state_machine, passiveOPEN);	
}

/* in the same vein, these are the functions that will be called
	during transitions between states, handled by the state machine */

/* 
tcp_connection_transition_passive_open
	will be called when the connection is transitioning from CLOSED with a passiveOPEN
	transition
*/
int tcp_connection_CLOSED_to_LISTEN(tcp_connection_t connection){
	puts("int tcp_connection_CLOSED_to_LISTEN");
	return 1;	
}

int tcp_connection_active_open(tcp_connection_t connection){

	return 1;
}
void tcp_connection_close(tcp_connection_t connection){

}
	

/********** End of State Changing Functions *******/

/********************* Sending Packets ********************/

// puts tcp_packet_data_t on to to_send queue
// returns 1 on success, -1 on failure (failure when queue actually already destroyed)
int tcp_connection_queue_ip_send(tcp_connection_t connection, tcp_packet_data_t packet_data){
	
	bqueue_t *to_send = connection->to_send;
	
	if(bqueue_enqueue(to_send, (void*)packet_data))
		return -1;
	
	return 1;
}

// pushes data to send_window for window to break into chunks which we can call get next on
// meant to be used before tcp_connection_send_next
void tcp_connection_push_data(tcp_connection_t connection, void* data, int num_bytes){

	send_window_t send_window = connection->send_window;
	// push data to send_window
	send_window_push(send_window, data, num_bytes);
}

//##TODO##
// helper to tcp_connection_send_next: handles sending the individual chunks returned from send_window
void tcp_connection_send_next_chunk(tcp_connection_t connection, send_window_chunk_t next_chunk){

	uint32_t host_port, dest_port;
	host_port = tcp_connection_get_local_port(connection);
	dest_port = tcp_connection_get_remote_port(connection);
	
	uint32_t seqnum = (uint32_t)next_chunk->seqnum; 
	void* data = next_chunk->data;
	int data_len = next_chunk->length;

	struct tcphdr* header = tcp_header_init(host_port, dest_port, seqnum, 0);
	
	// Todo: set anything else we need in header
	//TODO : SET WINDOW SIZE???
	
	// send off the packet -- it's ready!
	tcp_wrap_packet_send(connection, header, data, data_len);
}
// queues chunks off from send_window and handles sending them for as long as send_window wants to send more chunks
int tcp_connection_send_next(tcp_connection_t connection){
/*struct send_window_chunk{
	void* data;
	int length;
	int seqnum;
};*/	
	int bytes_sent = 0;
	send_window_chunk_t next_chunk;
	send_window_t send_window = connection->send_window;
	// get our chunk from the window
	next_chunk = send_window_get_next(send_window);
	
	// keep sending as many chunks as window has available to give us
	while(next_chunk != NULL){
	
		tcp_connection_send_next_chunk(connection, next_chunk);
		// increment bytes_sent
		bytes_sent = bytes_sent + next_chunk->length;
		
		// get next chunk if there is one
		next_chunk = send_window_get_next(send_window);
	}	
	return bytes_sent;
}

int tcp_connection_send_data(tcp_connection_t connection, const unsigned char* to_write, int num_bytes){
	
	// push data to window and then send as much as we can
	tcp_connection_push_data(connection, (void*)to_write, num_bytes);	
	// send as much data right now as send_window allows
	int ret = tcp_connection_send_next(connection);
	return ret;
}


/********************* End of Sending Packets ********************/

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

	
