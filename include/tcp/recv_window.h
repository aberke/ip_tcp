#ifndef __RECV_WINDOW_H__ 
#define __RECV_WINDOW_H__

#include <inttypes.h>

#include "utils.h"

#define MAX_SEQNUM ((unsigned)-1)

struct recv_chunk{
	uint32_t seqnum;
	void* data;
	int length;
};

typedef struct recv_chunk* recv_chunk_t;

recv_chunk_t recv_chunk_init(uint32_t seq, void* data, int l);

// this destroys the data it was holding!!
void recv_chunk_destroy(recv_chunk_t* rc);

//typedef struct recv_window_chunk* recv_window_chunk_t;
//recv_window_chunk_t recv_window_chunk_init(void* data, uint32_t length);
//void recv_window_chunk_destroy(recv_window_chunk_t* rwc);
//void recv_window_chunk_destroy_total(recv_window_chunk_t* rwc, destructor_f destructor);
//void recv_window_chunk_destroy_free(recv_window_chunk_t* rwc);

typedef struct recv_window* recv_window_t;
recv_window_t recv_window_init(uint16_t window_size, uint32_t ISN);
void recv_window_destroy(recv_window_t* recv_window);
/* allows us to decide if we should drop the packet or not right away 
	pass in length 0
	returns >=0 if valid seqnum (in window) <0 if invalid
*/
int recv_window_validate_seqnum(recv_window_t recv_window, uint32_t seqnum, uint32_t length);
memchunk_t recv_window_get_next(recv_window_t window, int bytes);
uint32_t recv_window_get_ack(recv_window_t window);
uint16_t recv_window_get_size(recv_window_t window);
void recv_window_receive(recv_window_t window, void* data, uint32_t length, uint32_t seqnum);

#endif // __RECV_WINDOW_H__
