#ifndef __WINDOW_H__ 
#define __WINDOW_H__

#include "utils.h"

#define WINDOW_CHUNK_SIZE 1024
#define MAX_SEQNUM ((unsigned)-1)

typedef struct send_window* send_window_t;

struct window_chunk{
	void* data;
	int length;
	int seqnum;
};

typedef struct window_chunk* window_chunk_t;

void window_chunk_destroy(window_chunk_t* wc);
void window_chunk_destroy_total(window_chunk_t* wc, destructor_f destructor);
void window_chunk_destroy_free(window_chunk_t* wc);

send_window_t send_window_init(double timeout, int send_window_size, int send_size, int ISN);
void send_window_destroy(send_window_t* send_window);

void send_window_push(send_window_t send_window, void* data, int length);
void send_window_check_timers(send_window_t send_window);
void send_window_ack(send_window_t send_window, int index);
void send_window_resize(send_window_t send_window, int size);
window_chunk_t send_window_get_next(send_window_t send_window);

void send_window_print(send_window_t send_window);

#endif // __WINDOW_H__
