#ifndef __TCP_CONNECTION_H__ 
#define __TCP_CONNECTION_H__

#include <inttypes.h>
#include "bqueue.h"

/* for tcp_packet_data_t HA! */
#include "ip_utils.h"
 
typedef struct tcp_connection* tcp_connection_t;  

tcp_connection_t tcp_connection_init(int socket, bqueue_t *to_send);
void tcp_connection_destroy(tcp_connection_t connection);

uint16_t tcp_connection_get_remote_port(tcp_connection_t connection);
uint16_t tcp_connection_get_local_port(tcp_connection_t connection);

void tcp_connection_set_local_port(tcp_connection_t connection, uint16_t port);

uint32_t tcp_connection_get_remote_ip(tcp_connection_t connection);
uint32_t tcp_connection_get_local_ip(tcp_connection_t connection);

int tcp_connection_get_socket(tcp_connection_t connection);

void tcp_connection_print_state(tcp_connection_t connection);

/****** Receiving packets **********/
void tcp_connection_handle_packet(tcp_connection_t connection, tcp_packet_data_t packet);
/****** End of Receiving Packets **********/

/******* Sending Packets **************/
int tcp_connection_send_data(tcp_connection_t connection, const unsigned char* to_write, int num_bytes);
/******* End of Sending Packets **************/

/********** State Changing Functions *************/

	/****** Functions called to invoke statemachine ******/
	int tcp_connection_passive_open(tcp_connection_t connection);
	/****** End of Functions called to invoke statemachine ******/
	
	/****** Functions called as actions by statemachine ******/
	int tcp_connection_CLOSED_to_LISTEN(tcp_connection_t connection);
	/****** End of Functions called as actions by statemachine ******/

/********** End of Sate Changing Functions *******/

/* for testing */
void tcp_connection_print(tcp_connection_t connection);
void tcp_connection_set_remote(tcp_connection_t connection, uint32_t remote_ip, uint16_t remote_port);


#endif //__TCP_CONNECTION_H__
