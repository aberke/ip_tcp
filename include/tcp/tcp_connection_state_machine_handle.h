#ifndef __TCP_CONNECTION_STATE_MACHINE_HANDLE_H__ 
#define __TCP_CONNECTION_STATE_MACHINE_HANDLE_H__

#include "tcp_connection.h"
#include "tcp_utils.h"
#include "state_machine.h"
#include "send_window.h"
#include "recv_window.h"
#include "int_queue.h"

//returns 1=true if in a closing state, 0=false otherwise
int tcp_connection_in_closing_state(tcp_connection_t connection);

/********** State Changing Functions *************/


/****** Functions called to invoke statemachine ******/
int tcp_connection_passive_open(tcp_connection_t connection);
int tcp_connection_active_open(tcp_connection_t connection, uint32_t ip_addr, uint16_t port);
/****** End of Functions called to invoke statemachine ******/
	
// to be called during invalid transition 
int tcp_connection_invalid_transition(tcp_connection_t connection);

// sometimes RFC specifies that if in a given state an action should be ignored
// eg, when in LISTEN and receive rst
int tcp_connection_NO_ACTION_transition(tcp_connection_t connection);

	/****** Functions called as actions by statemachine ******/
int tcp_connection_CLOSED_to_LISTEN(tcp_connection_t connection);
int tcp_connection_CLOSED_to_SYN_SENT(tcp_connection_t connection);

int tcp_connection_LISTEN_to_SYN_SENT(tcp_connection_t connection);
int tcp_connection_LISTEN_to_CLOSED(tcp_connection_t connection);	
int tcp_connection_LISTEN_to_SYN_RECEIVED(tcp_connection_t connection);	

/* helper to CLOSED_to_SYN_SENT as well as in the _handle_read_write thread for resending syn */
int tcp_connection_send_syn(tcp_connection_t connection);

int tcp_connection_SYN_SENT_to_CLOSED(tcp_connection_t connection);
int tcp_connection_SYN_SENT_to_CLOSED_by_RST(tcp_connection_t connection);
int tcp_connection_SYN_SENT_to_ESTABLISHED(tcp_connection_t connection);
int tcp_connection_SYN_SENT_to_SYN_RECEIVED(tcp_connection_t connection);
///##TODO##
int tcp_connection_SYN_RECEIVED_to_ESTABLISHED(tcp_connection_t connection);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o Closing Connection 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */
/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o Closing Connection 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */

// allows us to resend fin like we do for syn or any data
int tcp_connection_send_fin(tcp_connection_t connection);

// called when ready to ack a fin
int tcp_connection_ack_fin(tcp_connection_t connection);

/***pCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpC Passive Close pCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpC******/	

// called when receives FIN
int tcp_connection_receive_FIN(tcp_connection_t connection);

// it all starts when we receive a FIN
// we seem to need to do the exact same thing whether we were in SYN_RECEIVED or ESTABLISHED
int tcp_connection_transition_CLOSE_WAIT(tcp_connection_t connection);

int tcp_connection_CLOSE_WAIT_to_LAST_ACK(tcp_connection_t connection);

int tcp_connection_LAST_ACK_to_CLOSED(tcp_connection_t connection);

/*****aCaCaCaCaCaCaCaCaCaCaCaaCaCCaCaC Active Close aCaCaCaCaCaCaCaCaCaCaCaCaCaaCaCaCaCaCaC**********/

// before we call CLOSE we need to set this boolean!
void tcp_connection_set_close(tcp_connection_t connection);
// boolean 1 if closing, 0 otherwise
int tcp_connection_get_close_boolean(tcp_connection_t connection);

// called when user commands CLOSE
int tcp_connection_close(tcp_connection_t connection);


int tcp_connection_SYN_RECEIVED_to_FIN_WAIT_1(tcp_connection_t connection);

int tcp_connection_ESTABLISHED_to_CLOSE_WAIT(tcp_connection_t connection);
int tcp_connection_ESTABLISHED_to_FIN_WAIT_1(tcp_connection_t connection);

// they acked our fin but now we're waiting for their fin
int tcp_connection_FIN_WAIT_1_to_FIN_WAIT_2(tcp_connection_t connection);

int tcp_connection_FIN_WAIT_1_to_CLOSING(tcp_connection_t connection);

int tcp_connection_FIN_WAIT_2_to_TIME_WAIT(tcp_connection_t connection);

int tcp_connection_CLOSING_to_TIME_WAIT(tcp_connection_t connection);

int tcp_connection_TIME_WAIT_to_CLOSED(tcp_connection_t connection);

///////////////////////////////////////////////////////////////////////////////////
int tcp_connection_CLOSED_by_RST(tcp_connection_t connection);

// sometimes we just need to give up.  eg ABORT transition called in thread after fin never acked
/* send rst and transition to closed by ABORT */
int tcp_connection_ABORT(tcp_connection_t connection);

// there are times when the CLOSE call is illegal given the state - we should reflect this to user, yes? 
// and not pthread_cond_wait indefinitely?
int tcp_connection_CLOSING_error(tcp_connection_t connection);

/****** End of Functions called as actions by statemachine ******/


/********** End of Sate Changing Functions *******/





#endif //__TCP_CONNECTION_STATE_MACHINE_HANDLE_H__
