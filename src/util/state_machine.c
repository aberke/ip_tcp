		
/* state machine notes:
	The sequence number of the first data octet in this segment (except
    when SYN is present). If SYN is present the sequence number is the
    initial sequence number (ISN) and the first data octet is ISN+1.
*/	

#include <stdlib.h>

#include "utils.h"
#include "state_machine.h"
#include "array2d.h"

// debugging
#include "tcp_states.h"

static void _print(state_machine_t machine);

/* 
transitioning
	joins an action with a next state 
*/
transitioning_t transitioning_init(state_e s, action_f action){
	transitioning_t t = malloc(sizeof(struct transitioning));
	t->next_state = s;
	t->action = action;
	return t;
}

void transitioning_destroy(transitioning_t* t){
	free(*t);
	*t = NULL;
}

/* Some internal functions: */
void _set_transitioning(state_machine_t machine, state_e s, transition_e t, transitioning_t transitioning);
void _init(state_machine_t machine); 

/* The transition matrix = M has the property that M[i,j]
   is the state transitioned from when CURRENTLY in state i 
   if GIVEN transition j */

ARRAY_DEF(transitioning_t);

struct state_machine {
	ARRAY_TYPE(transitioning_t) transition_matrix;
	state_e current_state;
	void* argument;
};

/* init the array (it will be NUM_STATES x NUM_TRANSITIONS), and then set the current
	state to the start state */
state_machine_t state_machine_init(){
	state_machine_t state_machine = (struct state_machine*)malloc(sizeof(struct state_machine));
	ARRAY_INIT(state_machine->transition_matrix, transitioning_t, NUM_STATES, NUM_TRANSITIONS, START_STATE);
	state_machine->argument = NULL;
	state_machine->current_state = START_STATE;
	_init(state_machine);
	_print(state_machine);
	
	return state_machine;
}

/* destroy the array, and yourself */
void state_machine_destroy(state_machine_t* state_machine){
	ARRAY_DESTROY_TOTAL(&((*state_machine)->transition_matrix), transitioning_destroy);

	free(*(state_machine));
	*state_machine = NULL;
}

void state_machine_set_argument(state_machine_t state_machine, void* arg){
	state_machine->argument = arg;
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
int state_machine_transition(state_machine_t machine, transition_e t){
	printf("Transition from ");
	print_transition(t);
	printf(",");
	state_machine_print_state(machine);
	printf("-->");

	transitioning_t transitioning = ARRAY_GET(machine->transition_matrix, machine->current_state, t);	
	machine->current_state = transitioning->next_state;

	state_machine_print_state(machine);
	printf("\n");

	if(transitioning->action)
		return transitioning->action(machine->argument);	
	return NO_TRANSITION;
}

/* new state just returns the current state */
state_e state_machine_get_state(state_machine_t machine){
	return machine->current_state;
}

//// INTERNAL FUNCTIONS ////
static void _print(state_machine_t machine){
	int i,j;
	transitioning_t transition;
	for(i=0;i<1;i++){//NUM_STATES;i++){
		for(j=0;j<NUM_TRANSITIONS;j++){
			transition = ARRAY_GET(machine->transition_matrix, (state_e)i, (transition_e)j);
			print_transition((transition_e)j);
			printf(",");
			print_state((state_e)i);
			printf("-->");
			print_state(transition->next_state);
			printf("    ");
		}
		printf("\n");
	}
}
/* set state iterates through the states/transitions and for each
	calls the function get_next_state which will give the state that 
	should be at TransitionMatrix<state,transition>. Again, 
	TransitionMatrix[i,j] is the 
	NEXT STATE that the machine should move to if it is currently in
	state i and receives transition j */ 

void _init(state_machine_t machine){
	int i,j;
	transitioning_t transition;
	for(i=0;i<NUM_STATES;i++){
		for(j=0;j<NUM_TRANSITIONS;j++){
			
			transition = get_next_state((state_e)i, (transition_e)j);
			_set_transitioning(machine, (state_e)i, (transition_e)j, transition);

		}

		printf("Finished state %d\n\n", i);
		_print(machine);
		printf("\n\n");
	}
}
			
/* wraps around the ARRAY functionality that we're using here */
void _set_transitioning(state_machine_t machine, state_e state, transition_e transition, transitioning_t t){

	ARRAY_PUT(machine->transition_matrix, state, transition, t);
	
	transitioning_t t1 = ARRAY_GET(machine->transition_matrix, (state_e)0, (transition_e)1);
	if(!t1){
		puts("0,1 not set");
		return;
	}
	
	print_transition(transition);
	printf(",");
	print_state(state);
	printf("    ");
	printf("(0,1) == ");
	print_state(t1->next_state);
	printf("\n");
	 
	printf("GETTING: ");
	print_transition(transition);
	printf(",");
	print_state(state);
	printf("-->");
	t1 = ARRAY_GET(machine->transition_matrix, state, transition);
	print_state(t1->next_state);
	printf("\n");
}	

void state_machine_print_state(state_machine_t state_machine){
	print_state(state_machine->current_state);
}



