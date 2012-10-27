#ifndef __TEST_STATES_H__
#define __TEST_STATES_H__

#include <stdio.h>
#include "states.h"

/* Helper print method */
#define PRINT_STR(s) printf("%s\n", s);

enum state{
	S1=0,
	S2,
	S3
};
#define NUM_STATES 3

enum transition{
	T1=0,
	T2
};
#define NUM_TRANSITIONS 2

#define START_STATE S1
#define EMPTY_STATE -1

void print_state(state_e s);
state_e get_next_state(state_e s, transition_e t);

#endif // __TEST_STATES_H__
