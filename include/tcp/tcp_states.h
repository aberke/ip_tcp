//include/tcp_states.h
#ifndef __TCP_STATES_H__ 
#define __TCP_STATES_H__

#include "states.h"

enum state{
	CLOSED,
	LISTEN,
	SYN_SENT,
	SYN_RECEIVED,
	ESTABLISHED,
	NONE // is this the best way to handle transitions that don't exist??
	// TODO:  FINISH STATES!  FOR TEARDOWN
};
#define NUM_STATES 5  //TODO: ADD MORE ONCE HAVE TEAR DOWN/CORNER CASES ETC

enum transition{
	passiveOPEN,
	activeOPEN,
	receiveSYN,
	receiveSYN_ACK,  //receive SYN+ACK at same time
	receiveACK
	// TODO:  FINISH TRANSITIONS!  FOR TEARDOWN/CORNER CASES
};
#define NUM_TRANSITIONS 5  //TODO: ADD MORE ONCE HAVE CORNER CASES/TEARDOWN ETC

#define START_STATE 5

void tcp_states_print_state(state_e s);
	
#endif // __TCP_STATES_H__
