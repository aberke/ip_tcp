#ifndef __WINDOW_H__ 
#define __WINDOW_H__

typedef struct window* window_t;

struct window_chunk{
	void* data;
	int   length;
	int   seqnum;
};

typedef struct window_chunk* window_chunk_t;

window_t window_init(int window_size, int timeout);
void window_destroy(window_t* window);

void window_push(window_t window, void* data);
void window_check_timers(window_w window);
void window_ack(window_t window, int index);
void* window_get(window_t window);

#endif // __WINDOW_H__
