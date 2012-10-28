#ifndef __RECV_WINDOW_H__ 
#define __RECV_WINDOW_H__

#include "utils.h"

#define MAX_SEQNUM ((unsigned)-1)

typedef struct recv_window* recv_window_t;

struct recv_window_chunk{
	void* data;
	int offset;
	int length;
};

typedef struct recv_window_chunk* recv_window_chunk_t;
recv_window_chunk_t recv_window_chunk_init(void* data, int length);
void recv_window_chunk_destroy(recv_window_chunk_t* rwc);
void recv_window_chunk_destroy_total(recv_window_chunk_t* rwc, destructor_f destructor);

recv_window_t recv_window_init(int window_size, int ISN);
void recv_window_destroy(recv_window_t* recv_window);
recv_window_chunk_t recv_window_get_next(recv_window_t window);
int recv_window_receive(recv_window_t window, void* data, int length, int seqnum);

#endif // __RECV_WINDOW_H__
