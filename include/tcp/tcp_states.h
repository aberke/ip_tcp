//include/tcp_states.h
#ifndef __TCP_STATES_H__ 
#define __TCP_STATES_H__

#include "states.h"

enum state{
	CLOSED=0,
	LISTEN,
	SYN_SENT,
	SYN_RECEIVED,
	ESTABLISHED,
	
	FIN_WAIT_1,
	FIN_WAIT_2,
	CLOSE_WAIT,
	TIME_WAIT,
	LAST_ACK,

	CLOSING
};
#define NUM_STATES 11 //TODO: ADD MORE ONCE HAVE TEAR DOWN/CORNER CASES ETC
#define START_STATE CLOSED

enum transition{
	passiveOPEN=0,
	activeOPEN,
	receiveSYN,
	receiveSYN_ACK,  //receive SYN+ACK at same time

	receiveACK,
	CLOSE,
	TIME_ELAPSED,
	receiveFIN,
	SEND
};
#define NUM_TRANSITIONS 9  //TODO: ADD MORE ONCE HAVE CORNER CASES/TEARDOWN ETC

struct transitioning {
	state_e next_state;
	action_f action;
};

void tcp_states_print_state(state_e s);
	
#endif // __TCP_STATES_H__
