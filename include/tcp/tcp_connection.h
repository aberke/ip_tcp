#ifndef __TCP_CONNECTION_H__ 
#define __TCP_CONNECTION_H__

#include <inttypes.h>
#include <netinet/tcp.h>
#include "bqueue.h"
#include "send_window.h"
#include "recv_window.h"
#include "state_machine.h"

/* for tcp_packet_data_t HA!
#include "ip_utils.h"
#include "tcp_utils.h" */
#include "tcp_node.h"
#include "queue.h"

#define SIGNAL_CRASH_AND_BURN -777
#define SIGNAL_DESTROYING -666

typedef struct tcp_connection* tcp_connection_t;  

tcp_connection_t tcp_connection_init(tcp_node_t tcp_node, int socket, bqueue_t *to_send);
void tcp_connection_destroy(tcp_connection_t connection);

/* TODO: Start using this in our implemenation:
give to tcp_connection:

	tcp_connection	
		int ret_value; // return value for the calling tcp_api function
		pthread_mutex_t api_mutex
		pthread_cond_t api_cond
		// now when a tcp_api function calls, it will lock the mutex, and wait on the api_cond for the 
		//tcp connection to finish its duties
*/
int tcp_connection_get_api_ret(tcp_connection_t connection);
		
/* port/ip getters/setters */
uint16_t tcp_connection_get_remote_port(tcp_connection_t connection);
uint16_t tcp_connection_get_local_port(tcp_connection_t connection);
void tcp_connection_set_local_port(tcp_connection_t connection, uint16_t port);
void tcp_connection_set_local_ip(tcp_connection_t connection, uint32_t ip);
void tcp_connection_set_remote(tcp_connection_t connection, uint32_t remote_ip, uint16_t remote_port);
void tcp_connection_set_remote_ip(tcp_connection_t connection, uint32_t remote_ip);
uint32_t tcp_connection_get_remote_ip(tcp_connection_t connection);
uint32_t tcp_connection_get_local_ip(tcp_connection_t connection);

/* signaling */
void tcp_connection_api_signal(tcp_connection_t connection, int ret);
void tcp_connection_api_lock(tcp_connection_t connection);
void tcp_connection_api_unlock(tcp_connection_t connection);
int tcp_connection_api_result(tcp_connection_t connection);

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

/*0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0 to_read Thread o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0*/

// runs thread for tcp_connection to handle sending/reading and keeping track of its packets/acks
void *_handle_read_send(void *tcpconnection);

/************* Functions regarding the accept queue ************************/
	/* The accept queue is initialized when the server goes into the listen state.  
		Destroyed when leaves LISTEN state 
		Each time a syn is received, a new tcp_connection is created in the SYN_RECEIVED state and queued */

void tcp_connection_accept_queue_init(tcp_connection_t connection);
void tcp_connection_accept_queue_destroy(tcp_connection_t connection);
//void tcp_connection_accept_queue_connect(tcp_connection_t connection, accept_queue_triple_t triple);
accept_queue_data_t tcp_connection_accept_queue_dequeue(tcp_connection_t connection);


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

int tcp_connection_state_machine_transition(tcp_connection_t connection, transition_e transition);
state_e tcp_connection_get_state(tcp_connection_t connection);
void tcp_connection_set_state(tcp_connection_t connection, state_e state);
void tcp_connection_print_state(tcp_connection_t connection);

/****** Receiving packets **********/


/* Called when connection in LISTEN state receives a syn.  
	Queues info necessary to create a new connection when accept called 
	returns 0 on success, negative if failed -- ie queue destroyed */
int tcp_connection_handle_syn(tcp_connection_t connection, 
		uint32_t local_ip,uint32_t remote_ip, uint16_t remote_port, uint32_t seqnum);


/* Function for tcp_node to call to place a packet on this connection's
	my_to_read queue for this connection to handle in its _handle_read_send thread 
	returns 1 on success, 0 on failure */
int tcp_connection_queue_to_read(tcp_connection_t connection, tcp_packet_data_t tcp_packet);

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
void tcp_connection_refuse_connection(tcp_connection_t connection, tcp_packet_data_t data);

// called by v_write
int tcp_connection_send_data(tcp_connection_t connection, const unsigned char* to_write, int num_bytes);
/******* End of Sending Packets **************/
//////////////////////////////////////////////////////////////////////////////////////

// to print when user calls 'sockets'
void tcp_connection_print_sockets(tcp_connection_t connection);

#endif //__TCP_CONNECTION_H__
