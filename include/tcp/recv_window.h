#ifndef __RECV_WINDOW_H__ 
#define __RECV_WINDOW_H__

typedef struct recv_window* recv_window_t;

recv_window_t recv_window_init( /* ARGUMENTS */ );
void recv_window_destroy(recv_window_t* recv_window);

#endif // __RECV_WINDOW_H__
