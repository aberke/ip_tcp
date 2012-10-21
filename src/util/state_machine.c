#include <stdlib.h>

#include "state_machine.h"
#include "array2d.h"
#include "config.h" /* Config needs to include a header that defines the following
					 		state_e 
							transition_e
					      	NUM_STATES
							START_STATE
							EMPTY_STATE
					      	NUM_TRANSITIONS
					      	get_next_state(state_e current_state, transition_e t);
					*/

/* Some internal functions: */
void _set_state(state_machine_t machine, state_e s, transition_e t, state_e next_state);
void _set_states(state_machine_t machine); // RELIES ON THE FUNCTION get_next_state, which should have already been declard in states.h


/* The transition matrix = M has the property that M[i,j]
   is the state transitioned from when CURRENTLY in state i 
   if GIVEN transition j */

ARRAY_DEF(state_e);

struct state_machine {
	ARRAY_TYPE(state_e) transition_matrix;
	state_e current_state;
};

state_machine_t state_machine_init(){
	state_machine_t state_machine = (struct state_machine*)malloc(sizeof(struct state_machine));
	ARRAY_INIT(state_machine->transition_matrix, state_e, NUM_STATES, NUM_TRANSITIONS, EMPTY_STATE);
	state_machine->current_state = START_STATE;
	_set_states(state_machine);
	
	return state_machine;
}

void state_machine_destroy(state_machine_t* state_machine){
	ARRAY_DESTROY(&((*state_machine)->transition_matrix));

	free(*(state_machine));
	*state_machine = NULL;
}

/* KEY FUNCTIONS -- these define the ADT. 
	state_machine_transition() takes a machine and a transition and changes
		the current state given the specifications in the state transition
		matrix. 

	state_machine_get_state() takes a machine and hands back the current state
*/

void state_machine_transition(state_machine_t machine, transition_e t){
	machine->current_state = ARRAY_GET(machine->transition_matrix, machine->current_state, t);	
}

state_e state_machine_get_state(state_machine_t machine){
	return machine->current_state;
}

//// INTERNAL FUNCTIONS ////

void _set_states(state_machine_t machine){
	int i,j;
	state_e next_state;
	for(i=0;i<NUM_STATES;i++){
		for(j=0;j<NUM_TRANSITIONS;j++){
			
			next_state = get_next_state((state_e)i, (transition_e)j);
			_set_state(machine, (state_e)i, (transition_e)j, next_state);

		}
	}
}
			
void _set_state(state_machine_t machine, state_e state, transition_e transition, state_e next_state){
	ARRAY_PUT(machine->transition_matrix, state, transition, next_state);
}	



