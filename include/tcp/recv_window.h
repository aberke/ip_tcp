#ifndef __RECV_WINDOW_H__ 
#define __RECV_WINDOW_H__

#include <inttypes.h>

#include "utils.h"

#define MAX_SEQNUM ((unsigned)-1)

typedef struct recv_window* recv_window_t;

struct recv_window_chunk{
	void* data;
	uint32_t offset;
	uint32_t length;
};

typedef struct recv_window_chunk* recv_window_chunk_t;
recv_window_chunk_t recv_window_chunk_init(void* data, uint32_t length);
void recv_window_chunk_destroy(recv_window_chunk_t* rwc);
void recv_window_chunk_destroy_total(recv_window_chunk_t* rwc, destructor_f destructor);
void recv_window_chunk_destroy_free(recv_window_chunk_t* rwc);

recv_window_t recv_window_init(uint32_t window_size, uint32_t ISN);
void recv_window_destroy(recv_window_t* recv_window);
recv_window_chunk_t recv_window_get_next(recv_window_t window);
uint32_t recv_window_get_ack(recv_window_t window);
uint32_t recv_window_get_size(recv_window_t window);
void recv_window_receive(recv_window_t window, void* data, uint32_t length, uint32_t seqnum);

#endif // __RECV_WINDOW_H__
