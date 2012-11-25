#ifndef __TCP_CONNECTION_H__ 
#define __TCP_CONNECTION_H__

#include <inttypes.h>
//#include <netinet/tcp.h> -- we define it in utils.h!
#include "bqueue.h"
#include "send_window.h"
#include "recv_window.h"
#include "state_machine.h"
#include "tcp_node.h"
#include "int_queue.h"

/* for api signaling */
#define SIGNAL_CRASH_AND_BURN -777
#define SIGNAL_DESTROYING -666
#define API_TIMEOUT -555
#define REMOTE_CONNECTION_CLOSED -444
#define CONNECTION_RESET -333
#define CONNECTION_CLOSED -222

#define KEEP_ALIVE_FREQUENCY 1 //we send a keep-alive message every second


#define SYN_COUNT_MAX 3 // how many syns we send before timing out
/* TIMEOUTS DEFINED BY RFC:
      Timeouts

        USER TIMEOUT
        RETRANSMISSION TIMEOUT
        TIME-WAIT TIMEOUT */

#define USER_TIMEOUT 300 //lets do 5 minutes
#define RETRANSMISSION_TIMEOUT 2 //replaced SYN_TIMEOUT -- 2 seconds at first, and doubles each time next syn_sent

/* Must wait for 2MSL during time-wait */
/*The TCP standard defines MSL as being a value of 120 seconds (2 minutes). 
In modern networks this is an eternity, so TCP allows implementations to choose a lower value 
if it is believed that will lead to better operation. */
#define MSL 60 //1 minute rather than 2
#define TIME_WAIT_TIMEOUT 2*MSL //that's what the RFC said to do

typedef struct tcp_connection* tcp_connection_t;  

tcp_connection_t tcp_connection_init(tcp_node_t tcp_node, int socket, bqueue_t *to_send);
void tcp_connection_destroy(tcp_connection_t connection);

/*
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

/* If waiting for api signal and trying to shutdown, we end up blocking -- need way out
	to be called before pthread_destroy on api thread
	sets ret to SIGNAL_DESTROYING --should be something else? */
void tcp_connection_api_cancel(tcp_connection_t connection);
void tcp_connection_api_signal(tcp_connection_t connection, int ret);
void tcp_connection_api_lock(tcp_connection_t connection);
void tcp_connection_api_unlock(tcp_connection_t connection);
int tcp_connection_api_result(tcp_connection_t connection);

int tcp_connection_get_socket(tcp_connection_t connection);

/******* Window getting and setting and destroying functions *********/
/*send_window_t tcp_connection_send_window_init(tcp_connection_t connection, double timeout, int send_window_size, int send_size, int ISN);

// we should destroy the window when we close connections
void tcp_connection_send_window_destroy(tcp_connection_t connection);
recv_window_t tcp_connection_recv_window_init(tcp_connection_t connection, uint32_t window_size, uint32_t ISN);
void tcp_connection_recv_window_destroy(tcp_connection_t connection);
*/
recv_window_t tcp_connection_get_recv_window(tcp_connection_t connection);
send_window_t tcp_connection_get_send_window(tcp_connection_t connection);

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

void tcp_connection_handle_syn(tcp_connection_t connection, tcp_packet_data_t packet);
void tcp_connection_handle_syn_ack(tcp_connection_t connection, tcp_packet_data_t packet);

/* Called when connection in LISTEN state receives a syn.  
	Queues info necessary to create a new connection when accept called 
	returns 0 on success, negative if failed -- ie queue destroyed */
int tcp_connection_handle_syn_LISTEN(tcp_connection_t connection, 
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
// sometimes we just need to give up.  eg ABORT transition called in thread after fin never acked
/* send rst and transition to closed by ABORT */
int tcp_connection_ABORT(tcp_connection_t connection);
// called by v_write
int tcp_connection_send_data(tcp_connection_t connection, const unsigned char* to_write, int num_bytes);
void tcp_connection_ack(tcp_connection_t connection, uint32_t ack);
/******* End of Sending Packets **************/
//////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////
/*********************************** CLOSING *****************************************/

/* Instead of destroying receive window we just keep track of whether or not it exists.
	We don't want to destroy it because we still need it to catch the data sent and keep track of sequence numbers
	in case we shut down the receive window while still in the established state
	 this is necessary for api call v_shutdown type 2 when we just need to close the reading portion of the connection
	 returns 1 on success, -1 on failure */
int tcp_connection_close_recv_window(tcp_connection_t connection);

// returns 1=true if connection has reading capabilities, 0=false otherwise
int tcp_connection_recv_window_alive(tcp_connection_t connection);





/*********************************** END OF CLOSING *****************************************/
//////////////////////////////////////////////////////////////////////////////////////


// to print when user calls 'sockets'
void tcp_connection_print_sockets(tcp_connection_t connection);

#endif //__TCP_CONNECTION_H__
