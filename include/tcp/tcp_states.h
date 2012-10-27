//include/tcp_states.h
#ifndef __TCP_STATES_H__ 
#define __TCP_STATES_H__

#include "states.h"

#define NUM_STATES 5  //TODO: ADD MORE ONCE HAVE TEAR DOWN/CORNER CASES ETC
#define NUM_TRANSITIONS 5  //TODO: ADD MORE ONCE HAVE CORNER CASES/TEARDOWN ETC

enum state{
	CLOSED,
	LISTEN,
	SYN_SENT,
	SYN_RECEIVED,
	ESTABLISHED,
	NONE // is this the best way to handle transitions that don't exist??
	// TODO:  FINISH STATES!  FOR TEARDOWN
};

enum transition{
	passiveOPEN,
	activeOPEN,
	receiveSYN,
	receiveSYN_ACK,  //receive SYN+ACK at same time
	receiveACK
	// TODO:  FINISH TRANSITIONS!  FOR TEARDOWN/CORNER CASES
};

void tcp_states_print_state(state_e s);
	
#endif // __TCP_STATES_H__
