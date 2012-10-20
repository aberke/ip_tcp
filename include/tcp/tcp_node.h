// stuct tcp_node owns a table described in WORKING_ON
#ifndef __TCP_NODE_H__ 
#define __TCP_NODE_H__

typedef struct tcp_node* tcp_node_t; 

/* Notice below outward facing commands mimic ip_node -- I think keeping to one pattern will help us stay organized */
tcp_node_t tcp_node_init(list_t* links);
void ip_node_destroy(tcp_node_t* ip_node);
void tcp_node_print(tcp_node_t tcp_node);

void tcp_node_start();
void tcp_node_stop();
/* ***************************** */





#endif //__TCP_NODE_H__