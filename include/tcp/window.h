#ifndef __WINDOW_H__ 
#define __WINDOW_H__

#include "utils.h"

#define WINDOW_CHUNK_SIZE 1024
#define MAX_SEQNUM ((unsigned)-1)

typedef struct window* window_t;

struct window_chunk{
	void* data;
	int length;
	int seqnum;
};

typedef struct window_chunk* window_chunk_t;

void window_chunk_destroy(window_chunk_t* wc);
void window_chunk_destroy_total(window_chunk_t* wc, destructor_f destructor);
void window_chunk_destroy_free(window_chunk_t* wc);

window_t window_init(double timeout, int window_size, int send_size, int ISN);
void window_destroy(window_t* window);

void window_push(window_t window, void* data, int length);
void window_check_timers(window_t window);
void window_ack(window_t window, int index);
void window_resize(window_t window, int size);
window_chunk_t window_get_next(window_t window);

void window_print(window_t window);

#endif // __WINDOW_H__
