//include/tcp_states.h
#ifndef __TCP_STATES_H__ 
#define __TCP_STATES_H__

#define NUM_STATES 5  //TODO: ADD MORE ONCE HAVE TEAR DOWN/CORNER CASES ETC
#define NUM_TRANSITIONS 5  //TODO: ADD MORE ONCE HAVE CORNER CASES/TEARDOWN ETC

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
};
	

      
#endif // __TCP_STATES_H__