
#include <stdio.h>
#include "states.h"
#include "tcp_states.h"


/*
enum state{
	CLOSED,
	LISTEN,
	SYN_SENT,
	SYN_RECEIVED,
	ESTABLISHED,
	NONE
	// TODO:  FINISH STATES!  FOR TEARDOWN
};
enum transition{
	passiveOPEN,
	activeOPEN,
	receiveSYN,
	receiveSYN_ACK,  //receive SYN+ACK at same time
	receiveACK
	// TODO:  FINISH TRANSITIONS!  FOR TEARDOWN/CORNER CASES
};*/

state_e closed_next_state(transition_e t){
	
	switch(t){
		case passiveOPEN:
			return LISTEN;
		case activeOPEN:
			return SYN_SENT;
			
		case receiveSYN:
			return NONE;
		case receiveSYN_ACK:
			return NONE;
		case receiveACK:
			return NONE;
	}
	return NONE;
}
state_e listen_next_state(transition_e t){
	
	switch(t){	
		case passiveOPEN:
			return NONE;
		case activeOPEN:
			return NONE;
			
		case receiveSYN:
			return SYN_RECEIVED;
			
		case receiveSYN_ACK:
			return NONE;
		case receiveACK:
			return NONE;	
	}
	return NONE;
}
state_e syn_sent_next_state(transition_e t){
	
	switch(t){	
		case passiveOPEN:
			return NONE;
		case activeOPEN:
			return NONE;
				
		case receiveSYN:
			return SYN_RECEIVED;
		case receiveSYN_ACK:
			return ESTABLISHED;
			
		case receiveACK:
			return NONE; //right?
	}
	return NONE;
}
state_e syn_received_next_state(transition_e t){
	
	switch(t){
		case passiveOPEN:
			return NONE;
		case activeOPEN:
			return NONE;
		case receiveSYN:
			return NONE;
		case receiveSYN_ACK:
			return NONE;
			
		case receiveACK:
			return ESTABLISHED;
	}
	return NONE;
}
state_e established_next_state(transition_e t){
	/*	TODO: DEAL WITH ESTABLISHED STATE AND TEARDOWN */
	switch(t){
		case passiveOPEN:
			return NONE;
		case activeOPEN:
			return NONE;
		case receiveSYN:
			return NONE;
		case receiveSYN_ACK:
			return NONE;
		case receiveACK:
			return NONE;
	}
	return NONE;
}

state_e get_next_state(state_e s, transition_e t){
	
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
	}
	return NONE;
}
void tcp_states_print_state(state_e s){
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
	}
	printf("No Such State");
}

