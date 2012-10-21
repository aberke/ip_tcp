#ifndef __STATES_H__
#define __STATES_H__

typedef enum state state_e;
typedef enum transition transition_e;

state_e get_next_state(state_e s, transition_e t);

// OPTIONAL
void print_state(state_e s);
void print_transition(transition_e t);

#endif // __STATES_H__
