#ifndef __TCP_CONNECTION_STATE_MACHINE_HANDLE_H__ 
#define __TCP_CONNECTION_STATE_MACHINE_HANDLE_H__

#include "tcp_connection.h"
#include "tcp_utils.h"
#include "state_machine.h"
#include "send_window.h"
#include "recv_window.h"
#include "queue.h"


/********** State Changing Functions *************/


/****** Functions called to invoke statemachine ******/
int tcp_connection_passive_open(tcp_connection_t connection);
int tcp_connection_active_open(tcp_connection_t connection, uint32_t ip_addr, uint16_t port);
/****** End of Functions called to invoke statemachine ******/
	

	/****** Functions called as actions by statemachine ******/
int tcp_connection_CLOSED_to_LISTEN(tcp_connection_t connection);
int tcp_connection_CLOSED_to_SYN_SENT(tcp_connection_t connection);

int tcp_connection_LISTEN_to_SYN_SENT(tcp_connection_t connection);
int tcp_connection_LISTEN_to_CLOSED(tcp_connection_t connection);	
int tcp_connection_LISTEN_to_SYN_RECEIVED(tcp_connection_t connection);	

int tcp_connection_SYN_SENT_to_CLOSED(tcp_connection_t connection);
int tcp_connection_SYN_SENT_to_ESTABLISHED(tcp_connection_t connection);
int tcp_connection_SYN_SENT_to_SYN_RECEIVED(tcp_connection_t connection);

int tcp_connection_ESTABLISHED_to_CLOSE_WAIT(tcp_connection_t connection);
int tcp_connection_ESTABLISHED_to_FIN_WAIT_1(tcp_connection_t connection);

int tcp_connection_FIN_WAIT_1_to_CLOSING(tcp_connection_t connection);

int tcp_connection_CLOSE_WAIT_to_LAST_ACK(tcp_connection_t connection);

int tcp_connection_LAST_ACK_to_CLOSED(tcp_connection_t connection);

/****** End of Functions called as actions by statemachine ******/


/********** End of Sate Changing Functions *******/





#endif //__TCP_CONNECTION_STATE_MACHINE_HANDLE_H__