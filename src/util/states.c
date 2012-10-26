// states.c

#include "states.h"

/*
enum state{
	CLOSED,
	LISTEN,
	SYN_SENT,
	SYN_RECEIVED,
	ESTABLISHED
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
			return NULL;
		case receiveSYN_ACK:
			return NULL;
		case receiveACK:
			return NULL;
	}
	return NULL;
}
state_e listen_next_state(transition_e t){
	
	switch(t){
		case receiveSYN:
			return SYN_RECEIVED;		
	}
	return NULL;
}
state_e syn_sent_next_state(transition_e t){
	
	switch(t){		
		case receiveSYN:
			return SYN_RECEIVED;
		case receiveSYN_ACK:
			return ESTABLISHED;
		case receiveACK:
			return NULL //right?
	}
	return NULL;
}
state_e syn_received_next_state(transition_e t){
	
	switch(t){
		case receiveACK:
			return ESTABLISHED;
	}
	return NULL;
}
state_e established_next_state(transition_e t){
	/*	TODO: DEAL WITH ESTABLISHED STATE AND TEARDOWN
	switch(t){
		case passiveOPEN:
		
		case activeOPEN:
		
		case receiveSYN:
		
		case receiveSYN_ACK:
		
		case receiveACK:
		
	}*/
	return NULL;
}

state_e get_next_state(state_e s, transition_e t){
	
	switch(s){
		case CLOSED:
			return closed_next_state(t);
		case LISTEN:
			return listen_next_state(t);
		case SYN_SENT:
			return syn_sent_next_state(t);
		case SYN_RECEIVED:
			return syn_received_next_state(t);
		case ESTABLISHED:
			return established_next_state(t);
	}
	return NULL;
}

