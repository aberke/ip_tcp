		
/* state machine notes:
	The sequence number of the first data octet in this segment (except
    when SYN is present). If SYN is present the sequence number is the
    initial sequence number (ISN) and the first data octet is ISN+1.
*/	

#include <stdlib.h>

#include "utils.h"
#include "state_machine.h"
#include "array2d.h"

#define START_STATE CLOSED

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

/* init the array (it will be NUM_STATES x NUM_TRANSITIONS), and then set the current
	state to the start state */
state_machine_t state_machine_init(){
	state_machine_t state_machine = (struct state_machine*)malloc(sizeof(struct state_machine));
	ARRAY_INIT(state_machine->transition_matrix, state_e, NUM_STATES, NUM_TRANSITIONS, NONE);
	state_machine->current_state = START_STATE;
	_set_states(state_machine);
	
	return state_machine;
}

/* destroy the array, and yourself */
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

/* state_machine_transition takes the state of the current state_machine to the
	next state as dictated by the state transition matrix. It sets the current
	state to this new state. */
void state_machine_transition(state_machine_t machine, transition_e t){
	machine->current_state = ARRAY_GET(machine->transition_matrix, machine->current_state, t);	
}

/* new state just returns the current state */
state_e state_machine_get_state(state_machine_t machine){
	return machine->current_state;
}

//// INTERNAL FUNCTIONS ////

/* set state iterates through the states/transitions and for each
	calls the function get_next_state which will give the state that 
	should be at TransitionMatrix<state,transition>. Again, 
	TransitionMatrix[i,j] is the 
	NEXT STATE that the machine should move to if it is currently in
	state i and receives transition j */ 

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
			
/* wraps around the ARRAY functionality that we're using here */
void _set_state(state_machine_t machine, state_e state, transition_e transition, state_e next_state){
	ARRAY_PUT(machine->transition_matrix, state, transition, next_state);
}	

void state_machine_print_state(state_machine_t state_machine){
	print_state(state_machine->current_state);
}



