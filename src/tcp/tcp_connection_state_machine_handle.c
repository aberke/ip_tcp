
/***********************************************/
/*** #included from tcp_connection.c ***********/
/***********************************************/

//returns 1=true if in a closing state, 0=false otherwise
int tcp_connection_in_closing_state(tcp_connection_t connection){

	state_e s = tcp_connection_get_state(connection);
	if(s==FIN_WAIT_1 || s==FIN_WAIT_2 || s==CLOSE_WAIT || s==TIME_WAIT || s==LAST_ACK || s==CLOSING)
		return 1;
	return 0;
}


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
	print(("CLOSED --> LISTEN"), STATES_PRINT);
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
	print(("LISTEN --> SYN_RECEIVED"), STATES_PRINT);

	/* Must create new connection that will handle the rest of the connection establishment.  This connection will sit on
		the queue rather than being inserted into the connections array of the node.*/

	/*  1. ack their SEQ number 
	    2. send your own SEQ number */

	 //sets time that we transitioned to SYN_RECEIVED -- so that we can timeout when necessary
	gettimeofday(&(connection->state_timer), NULL);
	
	/*  just to reiterate, last_seq_received should have JUST been received by the SYN
		packet that made the state transition call this function */	// <-- Thanks a lot for that comment!! :)
	connection->receive_window = recv_window_init(DEFAULT_WINDOW_SIZE, connection->last_seq_received);
	connection->recv_window_alive = 1; // It's now alive! <-- need this boolean for implementing shutdown r option

	struct tcphdr* header = tcp_header_init(0);

	/* SYN */
	tcp_set_syn_bit(header);

	/* SEQ */
	
	/* Where we were initializing send windows was getting confusing, so I put it in tcp_connection_init
		so the ISN was set randomly upon initialization and is ready to be used for the first time here */
	uint32_t ISN = send_window_get_next_seq(connection->send_window);
	tcp_set_seq(header, ISN); 
	connection->last_seq_sent = ISN;

	tcp_set_window_size(header, recv_window_get_size(connection->receive_window));

	tcp_wrap_packet_send(connection, header, NULL, 0);

	return 1;
}


/* this function should actually be called ONLY by tcp_node, because 
	don't we need to first verify that this is a valid IP? */
int tcp_connection_active_open(tcp_connection_t connection, uint32_t ip_addr, uint16_t port){

	/* set the remote and then transition */
	tcp_connection_set_remote(connection, ip_addr, port);
	int result = state_machine_transition(connection->state_machine, activeOPEN);

	return result;
}

/* helper to CLOSED_to_SYN_SENT as well as in the _handle_read_write thread for resending syn */
int tcp_connection_send_syn(tcp_connection_t connection){

	//Note: Window already initialized with connection->last_seq_sent when we transitioned from CLOSED_to_SYN_SENT
	connection->syn_fin_count++;

	/* now init the packet */
	struct tcphdr* header = tcp_header_init(0);
	
	/* SYN */
	tcp_set_syn_bit(header);

	/* SEQ */
	tcp_set_seq(header, connection->last_seq_sent);
	
	// set time of when we're sending off syn
	gettimeofday(&(connection->state_timer), NULL);

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
	
	/* send random ISN as seqnum 
		Even though our send window was initialized with a random ISN it might be that we closed and are 
		reopening, and wouldn't want to put a similar seqnum in the network */
	uint32_t ISN = rand(); // only up to RAND_MAX, don't know what that is, but probably < SEQNUM_MAX	
	send_window_set_seq(connection->send_window, ISN);
	connection->last_seq_sent = ISN; //seq for syn about to be sent

	connection->syn_fin_count = 0;
	tcp_connection_send_syn(connection);

	return 1;
}

int tcp_connection_LISTEN_to_SYN_SENT(tcp_connection_t connection){
	print(("LISTEN --> SYN_SENT"), STATES_PRINT);

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

	struct tcphdr* header = tcp_header_init(0);

	/* SYN */
	tcp_set_syn_bit(header);

	/* SEQ */
	tcp_set_seq(header, send_window_get_next_seq(connection->send_window)); //NOTE: send window initalized in tcp_connection_init

	tcp_wrap_packet_send(connection, header, NULL, 0);
	
	return 1;
}

int tcp_connection_SYN_SENT_to_SYN_RECEIVED(tcp_connection_t connection){
	print(("SYN_SENT --> SYN_RECEIVED"), STATES_PRINT);

	/* I may be wrong, but if you sent a SYN and you received a SYN (and not
		a SYN/ACK, then this is a simultaneous connection. Send back a SYN/ACK */  //<-- yup exactly

	if(connection->receive_window != NULL){
		puts("Something went wrong, your receive window already exists and we just received a SYN. In tcp_connection_SYN_SENT_to_SYN_RECEIVED");
		exit(1); // CRASH AND BURN 
	}
	 //sets time that we transitioned to SYN_RECEIVED
	gettimeofday(&(connection->state_timer), NULL);
	
	// again, last_seq_received will have been set by the packet that triggered this transition 
	connection->receive_window = recv_window_init(DEFAULT_WINDOW_SIZE, connection->last_seq_received);
	connection->recv_window_alive = 1; // It's now alive! <-- need this boolean for implementing shutdown r option

	struct tcphdr* header = tcp_header_init(0); 
											
	/* SEQ */
	tcp_set_seq(header, connection->last_seq_sent);

	/* SYN */
	tcp_set_syn_bit(header);
	
	tcp_wrap_packet_send(connection, header, NULL, 0);

	return 1;
}

int tcp_connection_SYN_SENT_to_ESTABLISHED(tcp_connection_t connection){
	print(("SYN_SENT --> ESTABLISHED"), STATES_PRINT);

	/* just got a SYN/ACK, so send back a ACK, and that's it */

	if(connection->receive_window != NULL){
		puts("Something went wrong, your receive window already exists and we just received a SYN/ACK, in SYN_SENT-->ESTABLISHED");
		exit(1); // CRASH AND BURN
	}
		
	connection->receive_window = recv_window_init(DEFAULT_WINDOW_SIZE, connection->last_seq_received);
	connection->recv_window_alive = 1; // It's now alive! <-- need this boolean for implementing shutdown r option
	
	struct tcphdr* header = tcp_header_init(0);

	/* SEQ */
	tcp_set_seq(header, send_window_get_next_seq(connection->send_window));

	// send it off 
	tcp_wrap_packet_send(connection, header, NULL, 0);
	
	/* We can return from connect with success! */
	tcp_connection_api_signal(connection, 0); //sets api_ret to 0 for success
	
	return 1;
}

int tcp_connection_SYN_RECEIVED_to_ESTABLISHED(tcp_connection_t connection){
	print(("SYN_RECEIVED->ESTABLISHED"), STATES_PRINT);
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

// allows us to resend fin like we do for syn or any data
int tcp_connection_send_fin(tcp_connection_t connection){
	
	/* increment fin count */
	connection->syn_fin_count++;
	
	/* now init the packet */
	struct tcphdr* header = tcp_header_init(0);
	
	/* FIN */
	tcp_set_fin_bit(header);

	/* SEQ */
	tcp_set_seq(header, connection->fin_seqnum);
	
	// set time of when we're sending off fin
	gettimeofday(&(connection->state_timer), NULL);

	/*  that should be good? send it off. Note: NULL because I'm assuming there's
		to send when initializing a connection, but that's not necessarily true */
	tcp_wrap_packet_send(connection, header, NULL, 0);
	return 1;
}

// called when ready to ack a fin
int tcp_connection_ack_fin(tcp_connection_t connection){
	/* init the packet and set the ack bit -- tcp_wrap_packet_send will not reset the ack bit */
	struct tcphdr* header = tcp_header_init(0);
	tcp_set_ack(header, (connection->last_seq_received)+1);
	
	tcp_wrap_packet_send(connection, header, NULL, 0);
	
	return 1;
}

/***pCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpC Passive Close pCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpCpC******/	
// called when receives FIN
int tcp_connection_receive_FIN(tcp_connection_t connection){
	return state_machine_transition(connection->state_machine, receiveFIN);
}

int tcp_connection_ESTABLISHED_to_CLOSE_WAIT(tcp_connection_t connection){
	
	//SEND ACK
	struct tcphdr* header = tcp_header_init(0);
	/* SEQ */
	tcp_set_seq(header, send_window_get_next_seq(connection->send_window));
	// send it off 
	tcp_wrap_packet_send(connection, header, NULL, 0);
	
	/* We can return from anything we're waiting on to inform user of FIN */
	tcp_connection_api_signal(connection, REMOTE_CONNECTION_CLOSED); 	
	
	// inform user that remote connection closed
	printf("[socket %d]: Remote connection closed\n", connection->socket_id);
	// User is now supposed to tell connection to CLOSE
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
	connection->syn_fin_count = 0;
	return state_machine_transition(connection->state_machine, CLOSE);
}	
int tcp_connection_SYN_RECEIVED_to_FIN_WAIT_1(tcp_connection_t connection){

	/*  RFC: If no SENDs have been issued and there is no pending data to send,
      then form a FIN segment and send it, and enter FIN-WAIT-1 state;
      otherwise queue for processing after entering ESTABLISHED state.*/
	
	connection->fin_seqnum = send_window_get_next_seq(connection->send_window);
	tcp_connection_send_fin(connection);
	
	return 1;
}

// transition occurs when in established state user commands to CLOSE
int tcp_connection_ESTABLISHED_to_FIN_WAIT_1(tcp_connection_t connection){
	puts("HANDLE tcp_connection_ESTABLISHED_to_FIN_WAIT_1");
	/* RFC: Queue this until all preceding SENDs have been segmentized, then
      form a FIN segment and send it.  In any case, enter FIN-WAIT-1
      state.
	*/
	
	/* So our issue here is that we don't have a way of pushing a fin to our send window.
		Here is how I will handle that: 
			We'll set the fin count to 0: */
	connection->syn_fin_count = 0;
			/* Now that we're in the FIN_WAIT_1 state, the thread will check at each iteration
				if it needs to resend its fin.  When it ensures the timeout has occurred 
				 then it will resend a fin up to 3 times.  Since we're in the FIN_WAIT_1 state the user 
				 will not be able to add to the send-window queue, so we can set:  */			
	connection->fin_seqnum = send_window_get_next_seq(connection->send_window);
	tcp_connection_send_fin(connection);
	
	return 1;	
}
// they acked our fin but now we're waiting for their fin
int tcp_connection_FIN_WAIT_1_to_FIN_WAIT_2(tcp_connection_t connection){
	// just hangin out
	return 1;
}

int tcp_connection_FIN_WAIT_1_to_CLOSING(tcp_connection_t connection){
	// SEND ACK ONLY AFTER HAVE RELIABLY SENT ALL DATA
	return 1;	
}

int tcp_connection_FIN_WAIT_2_to_TIME_WAIT(tcp_connection_t connection){
	/* We had already sent our fin, they acked it, but now they finally decided to close too and 
		sent their fin
		so we ACK their fin and set timer for time_wait */
	
	//set timer
	gettimeofday(&(connection->state_timer), NULL);
	
	//ack their fin
	tcp_connection_ack_fin(connection);		
	
	
	return 1;
}

// there are a few different ways we could get here -- we handle them all the same, right?
/* It might be that we were already in TIME_WAIT and get the fin again -- well it was a retransmission
	so lets ack it again and reset timer */
int tcp_connection_transition_TIME_WAIT(tcp_connection_t connection){
	
	//TODO: SET TIMER
	// ACK FIN
	
	return 1;
}
/* This closing process finally over -- TIME_ELAPSED occurred in TIME_WAIT so signal user safe to delete TCB */
int tcp_connection_TIME_WAIT_to_CLOSED(tcp_connection_t connection){
	
	tcp_connection_api_signal(connection, 0); //0 for success, right?
	
	// do we want to stop that infinite thread loop?
	
	//anything else?
	return 1;		
}	

/*****aCaCaCaCaCaCaCaCaCaCaCaaCaCCaCaC End of Active Close aCaCaCaCaCaCaCaCaCaCaCaCaCaaCaCaCaCaCaC**********/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////	
/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o Closing Connection 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */
/* 0o0o0oo0o0o0o0o0o0o0o0o0o0o0o0o0o0o Closing Connection 0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o */


int tcp_connection_LISTEN_to_CLOSED(tcp_connection_t connection){	
	print(("LISTEN --> CLOSED"), STATES_PRINT);
	
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
	
	/* RFC: Delete the TCB and return "error:  closing" responses to any
      queued SENDs, or RECEIVEs. */

	print(("SYN_SENT --> CLOSED"), STATES_PRINT);

	if(connection->send_window)
		send_window_destroy(&(connection->send_window));
	if(connection->receive_window)
		recv_window_destroy(&(connection->receive_window));
	
	tcp_connection_api_signal(connection, -ETIMEDOUT); // return from connect() api call with timeout error
	//tcp_node_remove_connection_kernal(connection->tcp_node, connection); //<-- put it in api call

	return 1;
}
/* When we close the connection, we want to relinquish its resources, which
	means we want to talk to the node (free its port for reuse, reset its
	ip addresses) */
int tcp_connection_CLOSED_by_RST(tcp_connection_t connection){
	print(("CLOSED by RST"), STATES_PRINT);
	if(connection->send_window)
		send_window_destroy(&(connection->send_window));
	if(connection->receive_window)
		recv_window_destroy(&(connection->receive_window));
	
	tcp_connection_api_signal(connection, -CONNECTION_RESET);

	/* just closing up, nothing to do */
	return -ECONNREFUSED;	
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

// sometimes we just need to give up.  eg ABORT transition called in thread after fin never acked
int tcp_connection_ABORT(tcp_connection_t connection){

	connection->syn_fin_count = 0;

	if(connection->send_window)
		send_window_destroy(&(connection->send_window));
	if(connection->receive_window)
		recv_window_destroy(&(connection->receive_window));
	
	tcp_connection_api_signal(connection, -ETIMEDOUT);
	return 1;	
}

// sometimes RFC specifies that if in a given state an action should be ignored
int tcp_connection_NO_ACTION_transition(tcp_connection_t connection){
	return 1;
}


// invalid transition
int tcp_connection_invalid_transition(tcp_connection_t connection){
	print(("invalid transition"), STATES_PRINT);
	tcp_connection_api_signal(connection, INVALID_TRANSITION);
	return INVALID_TRANSITION;
}
