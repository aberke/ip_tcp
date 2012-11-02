#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include "states.h"
#include "tcp_states.h"
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
			return transitioning_init(CLOSED, NULL); //TODO: SUPPLY ACTION FOR BAD CALL
	}
}

transitioning_t listen_next_state(transition_e t){
	switch(t){	
		case receiveSYN:
			/* send SYN+ACK */
			return transitioning_init(SYN_RECEIVED, (action_f)tcp_connection_LISTEN_to_SYN_RECEIVED);
		case CLOSE:
			/* delete TCB */
			return transitioning_init(CLOSED, (action_f)tcp_connection_LISTEN_to_CLOSED);
		case SEND:
			/* send SYN */
			return transitioning_init(SYN_SENT, (action_f)tcp_connection_LISTEN_to_SYN_SENT);
			
		default:
			return transitioning_init(LISTEN, NULL);
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
			
		default:
			return transitioning_init(SYN_SENT, NULL);
	}
}

transitioning_t syn_received_next_state(transition_e t){
	switch(t){
		case receiveACK:
			/* must be the ACK of your SYN */
			return transitioning_init(ESTABLISHED, NULL);
		case CLOSE:
			/* send FIN */
			return transitioning_init(FIN_WAIT_1, NULL);

		default:
			return transitioning_init(SYN_RECEIVED, NULL);
	}
}

transitioning_t established_next_state(transition_e t){
	switch(t){
		case receiveFIN:
			/* send ACK */
			return transitioning_init(CLOSE_WAIT, (action_f)tcp_connection_ESTABLISHED_to_CLOSE_WAIT);
		case CLOSE:
			/* send FIN */
			return transitioning_init(FIN_WAIT_1, (action_f)tcp_connection_ESTABLISHED_to_FIN_WAIT_1);

		default:
			return transitioning_init(ESTABLISHED, NULL);
	}
}

transitioning_t fin_wait_1_next_state(transition_e t){
	switch(t){
		case receiveACK: 
			/* must be the ACK of your FIN */
			/* ACTION: none */
			return transitioning_init(FIN_WAIT_2, NULL);
		case receiveFIN:
			/* ACTION: send ACK */
			return transitioning_init(CLOSING, (action_f)tcp_connection_FIN_WAIT_1_to_CLOSING);
		
		default:
			return transitioning_init(FIN_WAIT_1, NULL);
	}
}

transitioning_t fin_wait_2_next_state(transition_e t){
	switch(t){
		case receiveFIN:
			/* ACTION: send ACK */	
			return transitioning_init(TIME_WAIT, NULL);
		
		default:
			return transitioning_init(FIN_WAIT_2, NULL);
	}
}

transitioning_t close_wait_next_state(transition_e t){
	switch(t){
		case CLOSE:
			/* send FIN */
			return transitioning_init(LAST_ACK, (action_f)tcp_connection_CLOSE_WAIT_to_LAST_ACK);

		default:
			return transitioning_init(CLOSE_WAIT, NULL);
	}
}

transitioning_t last_ack_next_state(transition_e t){
	switch(t){
		case receiveACK:
			/* must be ACK of your FIN */
			return transitioning_init(CLOSED, (action_f)tcp_connection_LAST_ACK_to_CLOSED);
		
		default:
			return transitioning_init(LAST_ACK, NULL);
	}
}

transitioning_t time_wait_next_state(transition_e t){	
	switch(t){
		case TIME_ELAPSED:
			return transitioning_init(CLOSED, NULL);
		
		default:
			return transitioning_init(TIME_WAIT, NULL);
	}
}

transitioning_t closing_next_state(transition_e t){
	switch(t){
		case receiveACK:
			/* must be ACK of your FIN */
			return transitioning_init(TIME_WAIT, NULL);
		
		default:
			return transitioning_init(CLOSING, NULL);
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
	printf("%d=", (int)s);
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
			printf("LAST_ACK");
			return;
		case CLOSING:
			printf("CLOSING");	
			return;
		}
}

