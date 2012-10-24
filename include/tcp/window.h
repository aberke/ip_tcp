#ifndef __WINDOW_H__ 
#define __WINDOW_H__

#include "utils.h"

#define WINDOW_CHUNK_SIZE 1024

typedef struct window* window_t;

struct window_chunk{
	memchunk_t chunk;
	int seqnum;
};

typedef struct window_chunk* window_chunk_t;

void window_chunk_destroy(window_chunk_t* wc);

window_t window_init(double timeout, int window_size);
void window_destroy(window_t* window);

void window_push(window_t window, memchunk_t chunk);
void window_check_timers(window_t window);
void window_ack(window_t window, int index);
window_chunk_t window_get_next(window_t window);

#endif // __WINDOW_H__
