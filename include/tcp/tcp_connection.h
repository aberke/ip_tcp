#ifndef __TCP_CONNECTION_H__ 
#define __TCP_CONNECTION_H__

#include <inttypes.h>
#include <netinet/tcp.h>
#include "bqueue.h"
#include "send_window.h"
#include "recv_window.h"
#include "state_machine.h"

/* for tcp_packet_data_t HA! */
#include "ip_utils.h"
#include "tcp_utils.h"

 // a tcp_connection in the listen state queues this triple on its accept_queue when
// it receives a syn.  Nothing further happens until the user calls accept at which point
// this triple is dequeued and a connection is initiated with this information
// the connection should then set its state to listen and go through the LISTEN_to_SYN_RECEIVED transition
typedef struct accept_queue_triple* accept_queue_triple_t;
accept_queue_triple_t accept_queue_triple_init(uint32_t remote_ip, uint16_t remote_port, uint32_t last_seq_received);
void accept_queue_triple_destroy(accept_queue_triple_t triple);


typedef struct tcp_connection* tcp_connection_t;  

tcp_connection_t tcp_connection_init(int socket, bqueue_t *to_send);
void tcp_connection_destroy(tcp_connection_t connection);

uint16_t tcp_connection_get_remote_port(tcp_connection_t connection);
uint16_t tcp_connection_get_local_port(tcp_connection_t connection);

void tcp_connection_set_local_port(tcp_connection_t connection, uint16_t port);

void tcp_connection_set_remote(tcp_connection_t connection, uint32_t remote_ip, uint16_t remote_port);

uint32_t tcp_connection_get_remote_ip(tcp_connection_t connection);
uint32_t tcp_connection_get_local_ip(tcp_connection_t connection);

int tcp_connection_get_socket(tcp_connection_t connection);

/******* Window getting and setting and destroying functions *********/
/*send_window_t tcp_connection_send_window_init(tcp_connection_t connection, double timeout, int send_window_size, int send_size, int ISN);
send_window_t tcp_connection_get_send_window(tcp_connection_t connection);
// we should destroy the window when we close connections
void tcp_connection_send_window_destroy(tcp_connection_t connection);

recv_window_t tcp_connection_recv_window_init(tcp_connection_t connection, uint32_t window_size, uint32_t ISN);
recv_window_t tcp_connection_get_recv_window(tcp_connection_t connection);
void tcp_connection_recv_window_destroy(tcp_connection_t connection);
*/

/******* End of Window getting and setting and destroying functions *********/

/************* Functions regarding the accept queue ************************/
	/* The accept queue is initialized when the server goes into the listen state.  
		Destroyed when leaves LISTEN state 
		Each time a syn is received, a new tcp_connection is created in the SYN_RECEIVED state and queued */


void tcp_connection_accept_queue_init(tcp_connection_t connection);
void tcp_connection_accept_queue_destroy(tcp_connection_t connection);
void tcp_connection_accept_queue_connect(tcp_connection_t connection, accept_queue_triple_t triple);
accept_queue_triple_t tcp_connection_accept_queue_dequeue(tcp_connection_t connection);


/************* End of Functions regarding the accept queue ************************/

void tcp_connection_set_last_seq_received(tcp_connection_t connection, uint32_t seq);

/*
uint32_t tcp_connection_get_last_seq_received(tcp_connection_t connection);
uint32_t tcp_connection_get_last_seq_sent(tcp_connection_t connection);

This function isn't needed and probably shouldn't exist, you shouldn't be 
able to access the state machine of the connection directly, you should
just be able to access it through the connection by calling get_state and
set_state directly on the connection

state_machine_t tcp_connection_get_state_machine(tcp_connection_t connection);
*/

void tcp_connection_state_machine_transition(tcp_connection_t connection, state_e state);
state_e tcp_connection_get_state(tcp_connection_t connection);
void tcp_connection_set_state(tcp_connection_t connection, state_e state);
void tcp_connection_print_state(tcp_connection_t connection);

/****** Receiving packets **********/
void tcp_connection_handle_receive_packet(tcp_connection_t connection, tcp_packet_data_t packet);
/****** End of Receiving Packets **********/
//////////////////////////////////////////////////////////////////////////////////////
/******* Sending Packets **************/

// puts tcp_packet_data_t on to to_send queue
// returns 1 on success, -1 on failure (failure when queue actually already destroyed)
int tcp_connection_queue_ip_send(tcp_connection_t connection, tcp_packet_data_t packet);

int tcp_wrap_packet_send(tcp_connection_t connection, struct tcphdr* header, void* data, int data_len);

// pushes data to send_window for window to break into chunks which we can call get next on
// meant to be used before tcp_connection_send_next
void tcp_connection_push_data(tcp_connection_t connection, void* to_write, int num_bytes);
//##TODO##
// queues chunks off from send_window and handles sending them for as long as send_window wants to send more chunks
int tcp_connection_send_next(tcp_connection_t connection);



// called by v_write
int tcp_connection_send_data(tcp_connection_t connection, const unsigned char* to_write, int num_bytes);
/******* End of Sending Packets **************/
//////////////////////////////////////////////////////////////////////////////////////

// to print when user calls 'sockets'
void tcp_connection_print_sockets(tcp_connection_t connection);


/* for testing */
void tcp_connection_print(tcp_connection_t connection);


#endif //__TCP_CONNECTION_H__
