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
#define DEFAULT_WINDOW_SIZE ((uint16_t)100)
#define DEFAULT_CHUNK_SIZE 100
#define RAND_ISN() rand()


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
	
	/* these really only need to be set when we send
	   off the first seq, just to make sure that the 
	   ack that we get back is valid */
	uint32_t last_seq_received;
	uint32_t last_seq_sent;	

	// needs reference to the to_send queue in order to queue its packets
	bqueue_t *to_send;	//--- tcp data for ip to send
};

tcp_connection_t tcp_connection_init(int socket, bqueue_t *tosend){
	// init state machine
	state_machine_t state_machine = state_machine_init();

	// init windows
	
	
	queue_t accept_queue = queue_init();
	queue_set_size(accept_queue, ACCEPT_QUEUE_DEFAULT_SIZE);
	
	tcp_connection_t connection = (tcp_connection_t)malloc(sizeof(struct tcp_connection));

	// let's do this the first time we send the SYN, just so if we try to send before that
	// we'll crash and burn because it's null

	//send_window_t send_window = send_window_init(WINDOW_DEFAULT_TIMEOUT, WINDOW_DEFAULT_SEND_WINDOW_SIZE, DEFAULT_WINDOW_SIZE, RAND_ISN);
	connection->send_window = NULL;
	//connection->receive_window = recv_window_init(DEFAULT_WINDOW_SIZE, RAND_ISN);
	connection->receive_window = NULL;

	connection->socket_id = socket;
	//zero out virtual addresses to start
	connection->local_addr.virt_ip = 0;
	connection->local_addr.virt_port = 0;
	connection->remote_addr.virt_ip = 0;
	connection->remote_addr.virt_port = 0;
	
	connection->state_machine = state_machine;
	connection->accept_queue = accept_queue;
	connection->to_send = tosend;
	
	/* 	I know that this will set it to a huge number and not
		actually -1, I learned my lesson. These variables
		are used to hold on to the seq/acks we've received
		before we hand the logic off to the state machine */	
	connection->last_seq_received = -1;
	connection->last_seq_sent 	  = -1;

	state_machine_set_argument(state_machine, connection);		

	return connection;
}

void tcp_connection_destroy(tcp_connection_t connection){

	// destroy windows
	if( connection->send_window ) send_window_destroy(&(connection->send_window));
	if( connection->receive_window ) recv_window_destroy(&(connection->receive_window));
	
	// destroy state machine
	state_machine_destroy(&(connection->state_machine));

	free(connection);
	connection = NULL;
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
	puts("CLOSED --> LISTEN");
	return 1;	
}

/*
tcp_connection_CLOSED_to_SYN_SENT 
	should handle sending the SYN when a connection is actively opened, ie trying
	to actively connect to someone. 
*/
int tcp_connection_CLOSED_to_SYN_SENT(tcp_connection_t connection){
	puts("CLOSED --> SYN_SENT");

	/* first pick a syn to send */
	uint32_t ISN = rand(); // only up to RAND_MAX, don't know what that is, but probably < SEQNUM_MAX

	connection->send_window = send_window_init(WINDOW_DEFAULT_TIMEOUT, WINDOW_DEFAULT_SEND_WINDOW_SIZE, DEFAULT_WINDOW_SIZE, ISN);

	/* now init the packet */
	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port, 0);
	
	/* fill the syn, and set the seqnum */
	tcp_set_syn_bit(header);
	tcp_set_seq(header, ISN);

	/*  that should be good? send it off. Note: NULL because I'm assuming there's
		to send when initializing a connection, but that's not necessarily true */
	tcp_wrap_packet_send(connection, header, NULL, 0);

	return 1;
}

/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o LISTEN 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */
	
/*
tcp_connection_LISTEN_to_SYN_RECEIVED
	this function just needs to handle generating the ISN, setting the receive window,
	and sending the SYN/ACK. It will generate an ISN for it's sending window and
	send that off. The sending window should have been NULL before this point, if it
	wasn't then it was init()ed somewhere else, which is probably a mistake */
int tcp_connection_LISTEN_to_SYN_RECEIVED(tcp_connection_t connection){	
	puts("LISTEN --> SYN_RECEIVED");

	/*  1. ack their SEQ number 
	    2. send your own SEQ number */

	if(connection->send_window != NULL){
		puts("sending window is not null when we're trying to send a SYN/ACK in tcp_connection_LISTEN_to_SYN_RECEIVED. why?");
		exit(1); // CRASH AND BURN
	}

	uint32_t ISN = RAND_ISN();	
	connection->send_window = send_window_init(WINDOW_DEFAULT_TIMEOUT, WINDOW_DEFAULT_SEND_WINDOW_SIZE, DEFAULT_WINDOW_SIZE, ISN);
	
	/*  just to reiterate, last_seq_received should have JUST been received by the SYN
		packet that made the state transition call this function */
	connection->receive_window = recv_window_init(DEFAULT_WINDOW_SIZE, connection->last_seq_received);

	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port, 0);

	tcp_set_syn_bit(header);
	tcp_set_ack_bit(header);

	tcp_set_ack(header, recv_window_get_ack(connection->receive_window));
	tcp_set_seq(header, ISN);

	tcp_set_window_size(header, DEFAULT_WINDOW_SIZE);

	tcp_wrap_packet_send(connection, header, NULL, 0);

	return 1;
}

int tcp_connection_LISTEN_to_CLOSED(tcp_connection_t connection){	
	puts("LISTEN --> CLOSED");

	/*  there's not much to do here, except for get rid of the 
	   	data you were buffering (from the other side?) and getting
	   	rid of the queued connections. For now, just return */
	return 1;
}

int tcp_connection_LISTEN_to_SYN_SENT(tcp_connection_t connection){
	puts("LISTEN --> SYN_SENT");

	/* I don't really understand this transition. Why were you in the 
		listen state, and then all of a sudden ACTIVELY send a syn? */
	uint32_t ISN = RAND_ISN();
	connection->send_window = send_window_init(WINDOW_DEFAULT_TIMEOUT, WINDOW_DEFAULT_SEND_WINDOW_SIZE, DEFAULT_WINDOW_SIZE, ISN);
	
	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port, 0);

	tcp_set_syn_bit(header);
	tcp_set_window_size(header, DEFAULT_WINDOW_SIZE);
	tcp_set_seq(header, ISN);

	tcp_wrap_packet_send(connection, header, NULL, 0);
	
	return 1;
}	

/* o00o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o00o SYN SENT o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0ooo0o0o0o */

int tcp_connection_SYN_SENT_to_SYN_RECEIVED(tcp_connection_t connection){
	puts("SYN_SENT --> SYN_RECEIVED");

	/* I may be wrong, but if you sent a SYN and you received a SYN (and not
		a SYN/ACK, then this is a simultaneous connection. Send back a SYN/ACK */

	if(connection->receive_window != NULL){
		puts("Something went wrong, your receive window already exists and we just received a SYN. In tcp_connection_SYN_SENT_to_SYN_RECEIVED");
		exit(1); // CRASH AND BURN 
	}

	// again, last_seq_received will have been set by the packet that triggered this transition 
	connection->receive_window = recv_window_init(DEFAULT_WINDOW_SIZE, connection->last_seq_received);

	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port, 0); 
											
	// should have been set by the last person who sent the
	// the first syn
	tcp_set_seq(header, connection->last_seq_sent);
	tcp_set_ack(header, recv_window_get_ack(connection->receive_window));
	tcp_set_window_size(header, DEFAULT_WINDOW_SIZE);
	
	tcp_set_syn_bit(header);
	tcp_set_ack_bit(header);
	
	tcp_wrap_packet_send(connection, header, NULL, 0);

	return 1;
}
										
int tcp_connection_SYN_SENT_to_ESTABLISHED(tcp_connection_t connection){
	puts("SYN_SENT --> ESTABLISHED");

	/* just got a SYN/ACK, so send back a ACK, and that's it */

	if(connection->receive_window != NULL){
		puts("Something went wrong, your receive window already exists and we just received a SYN/ACK, in SYN_SENT-->ESTABLISHED");
		exit(1); // CRASH AND BURN
	}

	connection->receive_window = recv_window_init(DEFAULT_WINDOW_SIZE, connection->last_seq_received);
	
	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port, 0);

	// load it up!
	tcp_set_ack_bit(header);
	tcp_set_ack(header, recv_window_get_ack(connection->receive_window));

	// window size?
	tcp_set_window_size(header, recv_window_get_size(connection->receive_window));
	
	
	// send it off 
	tcp_wrap_packet_send(connection, header, NULL, 0);
	
	return 1;
}

int tcp_connection_SYN_SENT_to_CLOSED(tcp_connection_t connection){
	puts("SYN_SENT --> CLOSED");

	/* you're just closing up, there's nothing to do */
	return 1;
}

/* this function should actually be called ONLY by tcp_node, because 
	don't we need to first verify that this is a valid IP? */
int tcp_connection_active_open(tcp_connection_t connection, uint32_t ip_addr, uint16_t port){
	tcp_connection_set_remote(connection, ip_addr, port);
	state_machine_transition(connection->state_machine, activeOPEN);
	return 1;
}

/********** End of State Changing Functions *******/
/* ////////////////////////////////////////////// */
/**************** Receiving Packets ***************/

/* 
validates an ACK when it's being received during 
establishing the connection 
*/
int _validate_ack(tcp_connection_t connection, uint32_t ack){
	/* the ACK is valid if it is equal to one more than the 
		last sequence number sent */
	if (ack != (connection->last_seq_sent + 1) % MAX_SEQNUM)
		return -1;

	return 0;
}

/*
tcp_connection_handle_receive_packet
	as needed. All meta-information (SYN, FIN, etc.) will probably be 
	passed in to the state-machine, which will take appropriate action
	with state changes. Therefore, the logic of this function should 
	mostly be concerned with passing off the data to the window/validating
	the correctness of the received packet (that it makes sense) 
*/
void tcp_connection_handle_receive_packet(tcp_connection_t connection, tcp_packet_data_t packet){
	struct tcphdr* header = (struct tcphdr*)packet->packet;
	
	if(tcp_syn_bit(header) && tcp_ack_bit(header)){	
		if(_validate_ack(connection, tcp_ack(header)) < 0){
			puts("Got invalid ACK! Discarding...");
			return;
		}

		/* received a SYN/ACK, record the seq you got, and validate
			that the ACK you received is correct */
		connection->last_seq_received = tcp_seqnum(header);
		
		state_machine_transition(connection->state_machine, receiveSYN_ACK);
	}

	else if(tcp_syn_bit(header)){
		/* got a SYN, set the last_seq_received, then pass off to state machine */
		connection->last_seq_received = tcp_seqnum(header);
	
		state_machine_transition(connection->state_machine, receiveSYN);
	}

	/* this will almost universally be true. */
	if(tcp_ack_bit(header)){
	
		/* careful, this might be NULL */
		send_window_ack(connection->send_window, tcp_ack(header));
	}
			
	/* Umm, anything else ? */	
}

/* 0o0o0oo0o0o0o0o0o0o0o SENDING o0o0o0ooo0o0o0o0o0o0o0o0o0oo0o */

/* 
tcp_connection_queue_ip_send
	this is the reason that tcp_wrap_packet_send needs to be defined 
	in tcp_connection.c, because otherwise tcp_utils would need to 
	#include or somehow know about the queue which tcp_connection
	is using. 
*/
int tcp_connection_queue_ip_send(tcp_connection_t connection, tcp_packet_data_t packet){
	return bqueue_enqueue(connection->to_send, packet);
}
	

/*
   NOTE should probably be here just because it's really would be a method 
   if we had classes, it takes in and relies upon the tcp_connection_t implementation 
	 
   I also left the original version intact in src/tcp/tcp_utils.s
*/
int tcp_wrap_packet_send(tcp_connection_t connection, struct tcphdr* header, void* data, int data_len){	
	//TODO: LAST STEP WITH READYING HEADER IS SETTING CHECKSUM -- SET CHECKSUM!

	/* data_len had better be the same size as when you called 
		tcp_header_init()!! */
	memcpy(header+tcp_offset_in_bytes(header), data, data_len);
	uint32_t total_length = tcp_offset_in_bytes(header) + data_len;

	// no longer need data
	free(data);
	
	// add checksum here?
	//tcp_utils_add_checksum(packet);
	tcp_utils_add_checksum(header);
	
	// send off to ip_node as a tcp_packet_data_t
	//tcp_packet_data_t packet_data = tcp_packet_data_init(packet, data_len+TCP_HEADER_MIN_SIZE, local_virt_ip, remote_virt_ip);
	tcp_packet_data_t packet_data = tcp_packet_data_init(
										(char*)header, 
										total_length,
										connection->local_addr.virt_ip,
										connection->remote_addr.virt_ip);
										
	// no longer need packet
	free(header);
	
	if(tcp_connection_queue_ip_send(connection, packet_data) < 0){
		//TODO: HANDLE!
		puts("Something wrong with sending tcp_packet to_send queue--How do we want to handle this??");	
		free(packet_data);
		return -1;
	}

	return 1;
}

void tcp_connection_send_next_chunk(tcp_connection_t connection, send_window_chunk_t next_chunk){
	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port, next_chunk->length);
	
	/* set the ack bit, and get the ack to send from 
		the window. ACK's should ALWAYS be sent (if established) */
	tcp_set_ack_bit(header);
	tcp_set_ack(header, recv_window_get_ack(connection->receive_window));
	
	/* the seqnum should be the seqnum in the next_chunk */
	tcp_set_seq(header, next_chunk->seqnum);
	
	/* send it off!  */
	tcp_wrap_packet_send(connection, header, next_chunk->data, next_chunk->length);
}

/* 
tcp_connection_push_data
	just pushes the given data into the sending window. Likely followed by a get_next loop 
*/
void tcp_connection_push_data(tcp_connection_t connection, void* data, int data_len){
	/* if the window doesn't exist, you can't write because you haven't 
		passed through the right states */
	if(connection->send_window == NULL) 
		return;

	send_window_push(connection->send_window, data, data_len);
}


// queues chunks off from send_window and handles sending them for as long as send_window wants to send more chunks
// NOTE: this presents the danger of one greedy connection that blocks all the other ones from sending
int tcp_connection_send_next(tcp_connection_t connection){

	int bytes_sent = 0;
	send_window_chunk_t next_chunk;
	send_window_t send_window = connection->send_window;

	// keep sending as many chunks as window has available to give us
	while((next_chunk = send_window_get_next(send_window))){
	
		// send it off
		tcp_connection_send_next_chunk(connection, next_chunk);

		// increment bytes_sent
		bytes_sent += next_chunk->length;
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
	/*  given the frequency of this call (whenever we send a chunk), 
		we should make it as efficient as possible, which includes
		pushing as little onto the stack as necessary */
	return connection->local_addr.virt_port;
/*	tcp_socket_address_t addr;
	addr = connection->local_addr;	
	uint16_t virt_port = addr.virt_port;
	return virt_port;
*/
}
uint16_t tcp_connection_get_remote_port(tcp_connection_t connection){
	/* see note above */
	return connection->remote_addr.virt_port;
/*	
	tcp_socket_address_t addr = connection->remote_addr;	
	uint16_t virt_port = addr.virt_port;
	return virt_port;
*/
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

	
