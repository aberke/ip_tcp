/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <inttypes.h>

#include "tcp_connection_state_machine_handle.h"
*/

/* 

I understand why these are now in a new function and I agreed with it, 
but I didn't realize that we would now have to add helper functions for
EVERYTHING, which I think can significantly bloat our code and also 
potentially decrease efficiency. I don't know if there's anything we
can do about that, but we should consiter it, because it now seems
really kind weird. One thing that we could do, in order to make these
separate files but to retain the ability to access directly members
of the tcp_connection struct, is that we could use the preprocessor
to paste the files together. I'm gonna go ahead and do it and see if
it works like I think it will (I'm just gonna try to add an include at 
the bottom of tcp_connection that #includes this file.

Ok I did it and it works and I think we should just to it this way, it'll make
the code much cleaner

NOTE: I've saved the old file as tcp_connection_state_machine_handle.c.old for 
	reference 

*/


/***********************************************/
/*** #included from tcp_connection.c ***********/
/***********************************************/


						/********** State Changing Functions *************/


/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o Establishing Connection 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */

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
	// init accept_queue
	bqueue_t *accept_queue = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(accept_queue);
	connection->accept_queue = accept_queue;
	return 1;	
}

/*
tcp_connection_LISTEN_to_SYN_RECEIVED
	this function just needs to handle generating the ISN, setting the receive window,
	and sending the SYN/ACK. It will generate an ISN for it's sending window and
	send that off. The sending window should have been NULL before this point, if it
	wasn't then it was init()ed somewhere else, which is probably a mistake */
int tcp_connection_LISTEN_to_SYN_RECEIVED(tcp_connection_t connection){	
	//puts("LISTEN --> SYN_RECEIVED");

	/* Must create new connection that will handle the rest of the connection establishment.  This connection will sit on
		the queue rather than being inserted into the connections array of the node.*/

	/*  1. ack their SEQ number 
	    2. send your own SEQ number */

	if(connection->send_window != NULL){
		CRASH_AND_BURN("sending window is not null when we're trying to send a SYN/ACK in tcp_connection_LISTEN_to_SYN_RECEIVED. why?");
	}

	uint32_t ISN = RAND_ISN();	
	connection->send_window = send_window_init(WINDOW_DEFAULT_TIMEOUT, WINDOW_DEFAULT_SEND_WINDOW_SIZE, DEFAULT_WINDOW_SIZE, ISN);

	// function does exactly that:
	//connection->send_window = send_window_init(WINDOW_DEFAULT_TIMEOUT, WINDOW_DEFAULT_SEND_WINDOW_SIZE, DEFAULT_WINDOW_SIZE, ISN);
	
	/*  just to reiterate, last_seq_received should have JUST been received by the SYN
		packet that made the state transition call this function */	// <-- Thanks a lot for that comment!! :)
	connection->receive_window = recv_window_init(DEFAULT_WINDOW_SIZE, connection->last_seq_received);

	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port, 0);

	tcp_set_syn_bit(header);
	tcp_set_ack_bit(header);

	tcp_set_ack(header, recv_window_get_ack(connection->receive_window));
	tcp_set_seq(header, ISN); 
	connection->last_seq_sent = ISN;

	tcp_set_window_size(header, DEFAULT_WINDOW_SIZE);

	tcp_wrap_packet_send(connection, header, NULL, 0);

	return 1;
}


/* this function should actually be called ONLY by tcp_node, because 
	don't we need to first verify that this is a valid IP? */
int tcp_connection_active_open(tcp_connection_t connection, uint32_t ip_addr, uint16_t port){

	/* set the remote and then transition */
	tcp_connection_set_remote(connection, ip_addr, port);
	state_machine_transition(connection->state_machine, activeOPEN);

	return 1;
}

/* helper to CLOSED_to_SYN_SENT as well as in the _handle_read_write thread for resending syn */
int tcp_connection_send_syn(tcp_connection_t connection){
	puts("sending syn");

	//Note: Window already initialized with connection->last_seq_sent when we transitioned from CLOSED_to_SYN_SENT

	/* now init the packet */
	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port, 0);
	
	/* fill the syn, and set the seqnum */
	tcp_set_syn_bit(header);
	tcp_set_seq(header, connection->last_seq_sent);
	printf("sending syn with seq: %u\n", connection->last_seq_sent);
	tcp_utils_add_checksum(header, sizeof(*header), connection->local_addr.virt_ip, connection->remote_addr.virt_ip, TCP_DATA);
	
	// set time of when we're sending off syn
	gettimeofday(&(connection->syn_timer), NULL);

	/*  that should be good? send it off. Note: NULL because I'm assuming there's
		to send when initializing a connection, but that's not necessarily true */
	tcp_wrap_packet_send(connection, header, NULL, 0);
	return 1;
}	

/*
tcp_connection_CLOSED_to_SYN_SENT 
	should handle sending the SYN when a connection is actively opened, ie trying
	to actively connect to someone. 
*/
int tcp_connection_CLOSED_to_SYN_SENT(tcp_connection_t connection){
	
	/* first pick a syn to send */
	uint32_t ISN = rand(); // only up to RAND_MAX, don't know what that is, but probably < SEQNUM_MAX	
	connection->last_seq_sent = ISN; //seq for syn about to be sent
	connection->send_window = send_window_init(WINDOW_DEFAULT_TIMEOUT, WINDOW_DEFAULT_SEND_WINDOW_SIZE, DEFAULT_WINDOW_SIZE, ISN);

	connection->syn_count = 1;
	tcp_connection_send_syn(connection);

	return 1;
}

int tcp_connection_LISTEN_to_SYN_SENT(tcp_connection_t connection){
	//puts("LISTEN --> SYN_SENT");

	// should we destroy the accept queue?  Like are we all done listening? -- lets check the RFC at some point...
	tcp_connection_accept_queue_destroy(connection);

	/* I don't really understand this transition. Why were you in the 
		listen state, and then all of a sudden ACTIVELY send a syn? */  //<-- yeah its a weird edge case
	/*  RFC pg 46: If no foreign socket was specified in the OPEN, but the
        connection is established (e.g., because a LISTENing connection
        has become specific due to a foreign segment arriving for the
        local socket), then the designated buffer is sent to the implied
        foreign socket.  Users who make use of OPEN with an unspecified
        foreign socket can make use of SEND without ever explicitly
        knowing the foreign socket address.

        However, if a SEND is attempted before the foreign socket
        becomes specified, an error will be returned*/
	uint32_t ISN = RAND_ISN();
	connection->send_window = send_window_init(WINDOW_DEFAULT_TIMEOUT, WINDOW_DEFAULT_SEND_WINDOW_SIZE, DEFAULT_WINDOW_SIZE, ISN);
	
	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port, 0);

	tcp_set_syn_bit(header);
	tcp_set_window_size(header, DEFAULT_WINDOW_SIZE);
	tcp_set_seq(header, ISN);
	tcp_set_ack(header, connection->last_seq_received);

	tcp_wrap_packet_send(connection, header, NULL, 0);
	
	return 1;
}

int tcp_connection_SYN_SENT_to_SYN_RECEIVED(tcp_connection_t connection){
	puts("SYN_SENT --> SYN_RECEIVED");

	/* I may be wrong, but if you sent a SYN and you received a SYN (and not
		a SYN/ACK, then this is a simultaneous connection. Send back a SYN/ACK */  //<-- yup exactly

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
	
	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port,0);

	// load it up!
	tcp_set_seq(header, connection->last_seq_sent + 1);
	tcp_set_ack_bit(header);
	//printf("setting ack to connection->last_seq_receved+1 = %u\n", ((connection->last_seq_received)+1));
	tcp_set_ack(header, ((connection->last_seq_received)+1));
	//tcp_set_ack(header, recv_window_get_ack(connection->receive_window));

	// window size?
	tcp_set_window_size(header, recv_window_get_size(connection->receive_window));
		
	// send it off 
	tcp_wrap_packet_send(connection, header, NULL, 0);
	
	/* We can return from connect with success! */
	tcp_connection_api_signal(connection, 0); //sets api_ret to 0 for success
	
	return 1;
}

int tcp_connection_SYN_RECEIVED_to_ESTABLISHED(tcp_connection_t connection){
	puts("SYN_RECEIVED->ESTABLISHED");
	//signal successfully tcp_api_accept to successfully return
	tcp_connection_api_signal(connection, tcp_connection_get_socket(connection)); 

	return 1;
}

/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o End of Establishing Connection 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o Closing Connection 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */
/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o Closing Connection 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */

/***pCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpC Passive Close pCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpC******/	
// called when receives FIN
int tcp_connection_receive_FIN(tcp_connection_t connection){
	return state_machine_transition(connection->state_machine, receiveFIN);
}

int tcp_connection_ESTABLISHED_to_CLOSE_WAIT(tcp_connection_t connection){
	//TODO: SEND ACK
	puts("Inform user that remote connection closed so that user can command CLOSE -- waiting for that CLOSE");
	// waits until user commands CLOSE to send FIN.  Is there a timeout??
	return 1;	
}

// transition occurs when in CLOSE_WAIT and user commands CLOSE	
int tcp_connection_CLOSE_WAIT_to_LAST_ACK(tcp_connection_t connection){
	//TODO: SEND FIN
	puts("HANDLE tcp_connection_CLOSE_WAIT_to_LAST_ACK -- need to send FIN and then wait for last ack before transitioning to CLOSED");
	return 1;	
}

int tcp_connection_LAST_ACK_to_CLOSED(tcp_connection_t connection){
	// destroy window
	send_window_destroy(&(connection->send_window));
	recv_window_destroy(&(connection->receive_window));
	puts("HANDLE tcp_connection_LAST_ACK_to_CLOSED -- All completed?");
	return 1;	
}
/***pCpCpCpCpCpCpCpCpCpCpCpCpCpCpCp End of Passive Close pCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpC******/
////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////	
/*****aCaCaCaCaCaCaCaCaCaCaCaaCaCCaCaC Active Close aCaCaCaCaCaCaCaCaCaCaCaCaCaaCaCaCaCaCaC**********/
// called when user commands CLOSE
int tcp_connection_close(tcp_connection_t connection){
	return state_machine_transition(connection->state_machine, CLOSE);
}	

// transition occurs when in established state user commands to CLOSE
int tcp_connection_ESTABLISHED_to_FIN_WAIT_1(tcp_connection_t connection){
	puts("HANDLE tcp_connection_ESTABLISHED_to_FIN_WAIT_1");
	/* RFC: Queue this until all preceding SENDs have been segmentized, then
      form a FIN segment and send it.  In any case, enter FIN-WAIT-1
      state.
	*/
	return 1;	
}

int tcp_connection_FIN_WAIT_1_to_CLOSING(tcp_connection_t connection){
	//TODO: SEND ACK
	puts("HANDLE tcp_connection_FIN_WAIT_1_to_CLOSING: TODO: SEND ACK");
	return 1;	
}

/*****aCaCaCaCaCaCaCaCaCaCaCaaCaCCaCaC End of Active Close aCaCaCaCaCaCaCaCaCaCaCaCaCaaCaCaCaCaCaC**********/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////	
/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o Closing Connection 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */
/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o Closing Connection 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */

//TODO
int tcp_connection_LISTEN_to_CLOSED(tcp_connection_t connection){	
	puts("LISTEN --> CLOSED");
	
	//TODO:
	
	/* RFC:   Any outstanding RECEIVEs are returned with "error:  closing"
      responses.  Delete TCB, enter CLOSED state, and return. */
	
	tcp_connection_accept_queue_destroy(connection);
	
	/*  there's not much to do here, except for get rid of the 
	   	data you were buffering (from the other side?) and getting
	   	rid of the queued connections. For now, just return */
	return 1;
}
										

/* I don't think this is right, because I think you could close the 
	connection after a syn sent without timing out (the user just 
	decides to close it and not wait for a response). This is the 
	function that should occur at THAT point */
int tcp_connection_SYN_SENT_to_CLOSED(tcp_connection_t connection){
	puts("SYN_SENT --> CLOSED");
	
	/* RFC: Delete the TCB and return "error:  closing" responses to any
      queued SENDs, or RECEIVEs. */
	
	
	if(connection->send_window)
		send_window_destroy(&(connection->send_window));
	if(connection->receive_window)
		recv_window_destroy(&(connection->receive_window));
	
	tcp_connection_api_signal(connection, -ETIMEDOUT); // return from connect() api call with timeout error
	tcp_node_remove_connection_kernal(connection->tcp_node, connection);
	/* you're just closing up, there's nothing to do */
	return 1;
}

int tcp_connection_SYN_RECEIVED_to_FIN_WAIT_1(tcp_connection_t connection){

	/*  RFC: If no SENDs have been issued and there is no pending data to send,
      then form a FIN segment and send it, and enter FIN-WAIT-1 state;
      otherwise queue for processing after entering ESTABLISHED state.*/

	return 1;
}

/* When we close the connection, we want to relinquish its resources, which
	means we want to talk to the node (free its port for reuse, reset its
	ip addresses) */
int tcp_connection_SYN_SENT_to_CLOSED_by_RST(tcp_connection_t connection){
	puts("SYN_SENT --> CLOSED by RST");
	if(connection->send_window)
		send_window_destroy(&(connection->send_window));
	if(connection->receive_window)
		recv_window_destroy(&(connection->receive_window));
	
	tcp_connection_api_signal(connection, -ECONNREFUSED);

	/* just closing up, nothing to do */
	return 1;
}
// there are times when the CLOSE call is illegal given the state - we should reflect this to user, yes? 
// and not pthread_cond_wait indefinitely?
int tcp_connection_CLOSING_error(tcp_connection_t connection){
	/*RFC: Respond with "error:  connection closing". */
	puts("TODO: make transition to handle: RFC: Respond with 'error:  connection closing'");
	tcp_connection_api_signal(connection, -EBADF); //fd isn't a valid open file descriptor.
	return -1;
}

/********** End of State Changing Functions *******/


