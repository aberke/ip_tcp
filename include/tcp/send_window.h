#ifndef __WINDOW_H__ 
#define __WINDOW_H__

#include "utils.h"
#include <inttypes.h>

#define WINDOW_CHUNK_SIZE 1024
#define MAX_SEQNUM ((unsigned)-1)

typedef struct send_window* send_window_t;

struct send_window_chunk{
	time_t send_time;
	void* data;
	int length;
	int seqnum;
	int offset;
};

typedef struct send_window_chunk* send_window_chunk_t;

void send_window_chunk_destroy(send_window_chunk_t* wc);
void send_window_chunk_destroy_total(send_window_chunk_t* wc, destructor_f destructor);
void send_window_chunk_destroy_free(send_window_chunk_t* wc);

void send_window_set_seq(send_window_t sc, uint32_t seq);

send_window_t send_window_init(double timeout, int send_window_size, int send_size, int ISN);
void send_window_destroy(send_window_t* send_window);

void send_window_set_size(send_window_t send_window, uint32_t size);
void send_window_push(send_window_t send_window, void* data, int length);
void send_window_check_timers(send_window_t send_window);
int send_window_validate_ack(send_window_t send_window, uint32_t ack);
void send_window_ack(send_window_t send_window, int index);
void send_window_resize(send_window_t send_window, int size);
uint32_t send_window_get_next_seq(send_window_t send_window);
send_window_chunk_t send_window_get_next(send_window_t send_window);

// needed for driver window_cmd
int send_window_get_size(send_window_t send_window);


void send_window_print(send_window_t send_window);

#endif // __WINDOW_H__
