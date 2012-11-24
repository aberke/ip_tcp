#include <stdlib.h>
#include <stdio.h>


#include "tcp_connection_state_machine_handle.h"

// TODO: HANDLE TODO'S FOR ERROR HANDLING

transitioning_t closed_next_state(transition_e t){
	switch(t){
		case passiveOPEN:
			/* create TCB */
			return transitioning_init(LISTEN, (action_f)tcp_connection_CLOSED_to_LISTEN);
		case activeOPEN:
			/* create TCB and send SYN */
			return transitioning_init(SYN_SENT, (action_f)tcp_connection_CLOSED_to_SYN_SENT);

		default:
			return transitioning_init(CLOSED, (action_f)tcp_connection_invalid_transition); //TODO: SUPPLY ACTION FOR BAD CALL
	}
}

transitioning_t listen_next_state(transition_e t){
	switch(t){	
		case receiveSYN:
			/* send SYN+ACK */
			/* Instead of transitioning this connection to SYN_RECEIVED, stay in LISTEN and
				create and queue (on accept_queue) a new tcp_connection that will sit in SYN_RECEIVED 
				and handle establishing the rest of the connection.  
				This is implemented in tcp_connection_LISTEN_to_SYN_RECEIVED
			*/	
			return transitioning_init(SYN_RECEIVED, (action_f)tcp_connection_LISTEN_to_SYN_RECEIVED);
		case CLOSE:
			/* delete TCB */
			return transitioning_init(CLOSED, (action_f)tcp_connection_LISTEN_to_CLOSED);
		case SEND:
			/* send SYN */
			return transitioning_init(SYN_SENT, (action_f)tcp_connection_LISTEN_to_SYN_SENT);
		case receiveRST:
			/* RFC says ignore rst when in LISTEN */
			return transitioning_init(LISTEN, (action_f)tcp_connection_NO_ACTION_transition);		
		case ABORT:
			return transitioning_init(CLOSED, (action_f)tcp_connection_NO_ACTION_transition);			
		default:
			return transitioning_init(LISTEN, (action_f)tcp_connection_invalid_transition);
	}
}

transitioning_t syn_sent_next_state(transition_e t){
	switch(t){	
		case receiveSYN:
			/* send ACK */
			return transitioning_init(SYN_RECEIVED, (action_f)tcp_connection_SYN_SENT_to_SYN_RECEIVED);
		case receiveSYN_ACK:
			/* send ACK */
			return transitioning_init(ESTABLISHED, (action_f)tcp_connection_SYN_SENT_to_ESTABLISHED);
		case CLOSE:
			/* delete TCB */
			return transitioning_init(CLOSED, (action_f)tcp_connection_SYN_SENT_to_CLOSED);

		case receiveRST:
			/* your connection was refused */
			return transitioning_init(CLOSED, (action_f)tcp_connection_CLOSED_by_RST);	
		case ABORT:
			return transitioning_init(CLOSED, (action_f)tcp_connection_NO_ACTION_transition);			
		default:
			return transitioning_init(SYN_SENT, (action_f)tcp_connection_invalid_transition);
	}
}

transitioning_t syn_received_next_state(transition_e t){
	switch(t){
		case receiveACK:
			/* must be the ACK of your SYN */
			return transitioning_init(ESTABLISHED, (action_f)tcp_connection_SYN_RECEIVED_to_ESTABLISHED);
		case receiveFIN:
			/* they closed on us -- we are now passive closers */
			return transitioning_init(CLOSE_WAIT, (action_f)tcp_connection_transition_CLOSE_WAIT);		
		case CLOSE:
			/* send FIN */
			return transitioning_init(FIN_WAIT_1, (action_f)tcp_connection_SYN_RECEIVED_to_FIN_WAIT_1);
		case receiveRST:
			/* your connection was refused */
			return transitioning_init(CLOSED, (action_f)tcp_connection_CLOSED_by_RST);	
		case ABORT:
			return transitioning_init(CLOSED, (action_f)tcp_connection_NO_ACTION_transition);		
		default:
			return transitioning_init(SYN_RECEIVED, (action_f)tcp_connection_invalid_transition);
	}
}

transitioning_t established_next_state(transition_e t){
	switch(t){
		case receiveFIN:
			/* they closed on us -- we are now passive closers: send ACK */
			return transitioning_init(CLOSE_WAIT, (action_f)tcp_connection_transition_CLOSE_WAIT);
		case CLOSE:
			/* send FIN */
			return transitioning_init(FIN_WAIT_1, (action_f)tcp_connection_ESTABLISHED_to_FIN_WAIT_1);
		case receiveRST:
			/* reset */
			return transitioning_init(CLOSED, (action_f)tcp_connection_CLOSED_by_RST);	
		case ABORT:
			return transitioning_init(CLOSED, (action_f)tcp_connection_NO_ACTION_transition);		
		default:
			return transitioning_init(ESTABLISHED, (action_f)tcp_connection_invalid_transition);
	}
}

transitioning_t fin_wait_1_next_state(transition_e t){
	switch(t){
		case receiveACK: 
			/* must be the ACK of your FIN */
			/* ACTION: none */
			return transitioning_init(FIN_WAIT_2, (action_f)tcp_connection_FIN_WAIT_1_to_FIN_WAIT_2);
		case receiveFIN:
			/* ACTION: send ACK */
			return transitioning_init(CLOSING, (action_f)tcp_connection_FIN_WAIT_1_to_CLOSING);
		case receiveRST:
			/* reset */
			return transitioning_init(CLOSED, (action_f)tcp_connection_CLOSED_by_RST);			
		case ABORT:
			/* we call this transition when they never ack our fin */
			return transitioning_init(CLOSED, (action_f)tcp_connection_NO_ACTION_transition);
		
		default:
			return transitioning_init(FIN_WAIT_1, (action_f)tcp_connection_invalid_transition);
	}
}

transitioning_t fin_wait_2_next_state(transition_e t){
	switch(t){
		case receiveFIN:
			/* ACTION: Finally - they're ready to close too! send ACK */	
			return transitioning_init(TIME_WAIT, (action_f)tcp_connection_FIN_WAIT_2_to_TIME_WAIT);
		
		case CLOSE:
			/*RFC:       Strictly speaking, this is an error and should receive a "error:
			  connection closing" response.  An "ok" response would be
			  acceptable, too, as long as a second FIN is not emitted (the first
			  FIN may be retransmitted though).*/
			return transitioning_init(TIME_WAIT, (action_f)tcp_connection_CLOSING_error);
		case receiveRST:
			/* reset */
			return transitioning_init(CLOSED, (action_f)tcp_connection_CLOSED_by_RST);			
		case ABORT:
			return transitioning_init(CLOSED, (action_f)tcp_connection_NO_ACTION_transition);				
		default:
			return transitioning_init(FIN_WAIT_2, (action_f)tcp_connection_invalid_transition);
	}
}

transitioning_t close_wait_next_state(transition_e t){
	switch(t){
		case CLOSE:
			/* RFC seems to contradict diagram :   Queue this request until all preceding SENDs have been
      		segmentized; then send a FIN segment, enter CLOSING state. */
			/* send FIN */
			return transitioning_init(LAST_ACK, (action_f)tcp_connection_CLOSE_WAIT_to_LAST_ACK);		
		case receiveFIN:
			/* Jeez another FIN? They must not have gotten our ack -- guess we've got to resend ack */
			return transitioning_init(CLOSE_WAIT, (action_f)tcp_connection_transition_CLOSE_WAIT);		
		case receiveRST:
			/* reset */
			return transitioning_init(CLOSED, (action_f)tcp_connection_CLOSED_by_RST);	
		case ABORT:
			return transitioning_init(CLOSED, (action_f)tcp_connection_NO_ACTION_transition);		
		default:
			return transitioning_init(CLOSE_WAIT, (action_f)tcp_connection_invalid_transition);
	}
}

transitioning_t last_ack_next_state(transition_e t){
	switch(t){
		case receiveACK:
			/* must be ACK of your FIN */
			return transitioning_init(CLOSED, (action_f)tcp_connection_LAST_ACK_to_CLOSED);
		
		case receiveFIN:
			/* RFC: Remain in the LAST-ACK state.  Is this a bad idea?? They want to close and so do we! 
				So here I'm resending the FIN */
			return transitioning_init(LAST_ACK, (action_f)tcp_connection_send_fin);
		case CLOSE: 
			/*RFC: Respond with "error:  connection closing". */
			return transitioning_init(TIME_WAIT, (action_f)tcp_connection_CLOSING_error);
		case receiveRST:
			/* reset */
			return transitioning_init(CLOSED, (action_f)tcp_connection_CLOSED_by_RST);				
		case TIME_ELAPSED:
			/* we'll call this transition in the thread after user time out because:
				RFC: If an ACK is not forthcoming, after the user timeout the connection is 
				aborted and the user is told.*/
			return transitioning_init(CLOSED, (action_f)tcp_connection_ABORT);		
		case ABORT:
			return transitioning_init(CLOSED, (action_f)tcp_connection_NO_ACTION_transition);				
		default:
			return transitioning_init(LAST_ACK, (action_f)tcp_connection_invalid_transition);
	}
}

transitioning_t time_wait_next_state(transition_e t){	
	switch(t){
		case TIME_ELAPSED:
			return transitioning_init(CLOSED, (action_f)tcp_connection_TIME_WAIT_to_CLOSED);
		
		case CLOSE: 
			/*RFC: Respond with "error:  connection closing". */
			return transitioning_init(TIME_WAIT, (action_f)tcp_connection_CLOSING_error);
		case receiveFIN:
			/* retransmission of the remote FIN.  Acknowledge it, and restart
          	the 2 MSL timeout. */
          	return transitioning_init(TIME_WAIT, (action_f)tcp_connection_FIN_WAIT_2_to_TIME_WAIT); 
		case ABORT:
			return transitioning_init(CLOSED, (action_f)tcp_connection_NO_ACTION_transition);		
		case receiveRST:
			/* reset */
			return transitioning_init(CLOSED, (action_f)tcp_connection_CLOSED_by_RST);			
		default:
			return transitioning_init(TIME_WAIT, (action_f)tcp_connection_invalid_transition);
	}
}

transitioning_t closing_next_state(transition_e t){
	switch(t){
		case receiveACK:
			/* must be ACK of your FIN */
			return transitioning_init(TIME_WAIT, (action_f)tcp_connection_CLOSING_to_TIME_WAIT);
		case receiveFIN:
			/* Remain in the CLOSE-WAIT state.  Maybe they didn't get our ack?  Here we resend it */
			return transitioning_init(CLOSING, (action_f)tcp_connection_FIN_WAIT_1_to_CLOSING);
		case CLOSE: 
			/*RFC: Respond with "error:  connection closing". */
			return transitioning_init(TIME_WAIT, (action_f)tcp_connection_CLOSING_error);
		case receiveRST:
			/* reset */
			return transitioning_init(CLOSED, (action_f)tcp_connection_CLOSED_by_RST);			
		case ABORT:
			return transitioning_init(CLOSED, (action_f)tcp_connection_NO_ACTION_transition);			
		default:
			return transitioning_init(CLOSING, (action_f)tcp_connection_invalid_transition);
	}
}

transitioning_t get_next_state(state_e s, transition_e t){
	switch(s){
		case CLOSED:
			return closed_next_state(t); break;
		case LISTEN:
			return listen_next_state(t); break;
		case SYN_SENT:
			return syn_sent_next_state(t); break;
		case SYN_RECEIVED:
			return syn_received_next_state(t); break;
		case ESTABLISHED:
			return established_next_state(t); break;
		case FIN_WAIT_1:
			return fin_wait_1_next_state(t); break;
		case FIN_WAIT_2:
			return fin_wait_2_next_state(t); break;
		case CLOSE_WAIT:
			return close_wait_next_state(t); break;
		case LAST_ACK:	
			return last_ack_next_state(t); break;
		case TIME_WAIT:
			return time_wait_next_state(t); break;
		case CLOSING:
			return closing_next_state(t); break;
		
		default:
			return NULL;
	}
}

void print_transition(transition_e t){
	printf("%d", (int)t);
}

void print_state(state_e s){
	switch(s){
		case CLOSED:
			printf("CLOSED");
			return;
		case LISTEN:
			printf("LISTEN");
			return;
		case SYN_SENT:
			printf("SYN_SENT");
			return;
		case SYN_RECEIVED:
			printf("SYN_RECEIVED");
			return;
		case ESTABLISHED:
			printf("ESTABLISHED");
			return;	
		case FIN_WAIT_1:
			printf("FIN_WAIT_1");
			return;
		case FIN_WAIT_2:
			printf("FIN_WAIT_2");
			return;
		case CLOSE_WAIT:
			printf("CLOSE_WAIT");	
			return;
		case TIME_WAIT:
			printf("TIME_WAIT");
			return;
		case LAST_ACK:
			printf("LAST_ACK\n");
			return;
		case CLOSING:
			printf("CLOSING\n");	
			return;
		}
}

