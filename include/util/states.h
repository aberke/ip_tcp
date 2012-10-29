#ifndef __STATES_H__
#define __STATES_H__

#include "utils.h"

typedef enum state state_e;
typedef enum transition transition_e;
typedef struct transitioning* transitioning_t;

transitioning_t transitioning_init(state_e next, action_f action);
void transitioning_destroy(transitioning_t* t);

transitioning_t get_next_state(state_e s, transition_e t);

void print_state(state_e s);
void print_transition(transition_e t);

#endif // __STATES_H__
