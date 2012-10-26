#ifndef __STATE_MACHINE_H__ 
#define __STATE_MACHINE_H__

#include "states.h"

// each tcp_connection has a statemachine keeping track of its state

typedef struct state_machine* state_machine_t;

state_machine_t state_machine_init();
void state_machine_destroy(state_machine_t* state_machine);

void state_machine_transition(state_machine_t machine, transition_e transition);
state_e state_machine_get_state(state_machine_t machine);

#endif // __STATE_MACHINE_H__
