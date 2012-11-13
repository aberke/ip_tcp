// stuct tcp_node owns a table described in WORKING_ON
#ifndef __TCP_NODE_H__ 
#define __TCP_NODE_H__

#include <inttypes.h>
#include "tcp_utils.h"

// is this okay to do?  Since they're mutually dependent we had a circle going on...
typedef struct tcp_node* tcp_node_t;

#include "ip_node.h"
#include "list.h"
#include "tcp_api.h"

/* set artificially low right now so we can make sure have no segfaults if ever reach limit */
#define MAX_FILE_DESCRIPTORS 5 // per process limit commonly set to 1024 on mac and linux machines

//// some helpful static globals
#define IP_HEADER_SIZE sizeof(struct ip)
#define UDP_PACKET_MAX_SIZE 1400
#define STDIN fileno(stdin)
#define MTU (UDP_PACKET_MAX_SIZE - IP_HEADER_SIZE)

 

// forward declare
struct tcp_api_args;
struct tcp_connection;


/*
// following structs defined in ip_node.h
//alex created ip_thread_data struct to pass in following arguments to start:
	struct ip_thread_data{
		ip_node_t ip_node;
		bqueue_t *to_send;
		bqueue_t *to_read;
		bqueue_t *stdin_commands;   // way for tcp_node to pass user input commands to ip_node
	};
// data type that to_send and to_read queues will store (ie queue and dequeue) -- need vip's associated with packet
typedef struct tcp_packet_data{
	uint32_t local_virt_ip;
	uint32_t remote_virt_ip;
	char packet[MTU];
	int packet_size;  //size of packet in bytes
} tcp_packet_data_t;
*/

/* Notice below outward facing commands mimic ip_node -- I think keeping to one pattern will help us stay organized */
tcp_node_t tcp_node_init(iplist_t* links);
void tcp_node_destroy(tcp_node_t ip_node);
void tcp_node_print(tcp_node_t tcp_node);

void tcp_node_start(tcp_node_t tcp_node);

// returns tcp_node->running
int tcp_node_running(tcp_node_t tcp_node);
/* ***************************** */

/******** Commands Regarding Kernal **********************/

// returns next available, currently unused, virtual socket file descriptor to initiate a new tcp_connection with
int tcp_node_next_virt_socket(tcp_node_t tcp_node);
// returns next available, currently unused, port to bind or connect/accept a new tcp_connection with
int tcp_node_next_port(tcp_node_t tcp_node);
// creates a new tcp_connection and properly places it in kernal table -- ports and ips initialized to 0
struct tcp_connection* tcp_node_new_connection(tcp_node_t tcp_node);


//needs to be called when close connection so that we can return port/socket to available queue for reuse
void tcp_node_return_socket_to_kernal(tcp_node_t tcp_node, int socket);

//needs to be called when close connection so that we can return port/socket to available queue for reuse
void tcp_node_return_port_to_kernal(tcp_node_t tcp_node, int port);

//ALEX TODO:
// gracefully CLOSE all connections
void tcp_node_close_all_connections(tcp_node_t tcp_node);

//###TODO: FINISH LOGIC ####
// use in node destroy??? to close better?
//needs to be called when close connection so that we can return port/socket to available queue for reuse
// returns 0 for now? //new number of connections in kernal
int tcp_node_close_connection(tcp_node_t tcp_node, struct tcp_connection* connection);

// removes connection from kernal
// returns number of connections still in kernal
int tcp_node_remove_connection_kernal(tcp_node_t tcp_node, struct tcp_connection* connection);
// returns 1 if the port is available for use, 0 if already in use
int tcp_node_port_unused(tcp_node_t tcp_node, int port);

// assigns port to tcp_connection and puts entry in hash table that hashes ports to tcp_connections
// returns 1 if port successfully assigned, 0 otherwise
int tcp_node_assign_port(tcp_node_t tcp_node, struct tcp_connection* connection, int port);

// returns tcp_connection corresponding to socket
struct tcp_connection* tcp_node_get_connection_by_socket(tcp_node_t tcp_node, int socket);

/******** End of Commands Regarding Kernal **********************/

/************************************ These commands currently unused due to Neil's commands below being used instead ****/
// puts command on to stdin_commands queue
// returns 1 on success, -1 on failure (failure when queue actually already destroyed)
int tcp_node_queue_ip_cmd(tcp_node_t tcp_node, char* buffered_cmd);
// puts command on to to_send queue
// returns 1 on success, -1 on failure (failure when queue actually already destroyed)
int tcp_node_queue_ip_send(tcp_node_t tcp_node, char* buffered_cmd);
/***************************************************************************************/

// returns whether ip_node running still
int tcp_node_ip_running(tcp_node_t tcp_node);
	
/* ADDED BY NEIL: tells the tcp_node to pass the command on to the ip_node */
void tcp_node_command_ip(tcp_node_t tcp_node, const char* command);

/* ADDED BY NEIL: stops everything */
void tcp_node_stop(tcp_node_t tcp_node);

/* ADDED BY NEIL: sends packet to ip */
void tcp_node_send(tcp_node_t tcp_node, char* to_write, int socket, uint32_t num_bytes);

/* ADDED BY NEIL : informs a remote connection of invalid port */
void tcp_node_invalid_port(tcp_node_t tcp_node, tcp_packet_data_t packet);
void tcp_node_refuse_connection(tcp_node_t tcp_node, tcp_packet_data_t packet);

/*********** For accept ************************************************/
// calls on the listening_connection to dequeue its triple and node creates new connection with information
// returned int is the new socket assigned to that new connection.  The connection finishes its handshake to get to
// 	established state
struct tcp_connection* tcp_node_connection_accept(tcp_node_t tcp_node, struct tcp_connection* listening_connection);

/*********** For use by tcp_node to reach ip_node items ****************/
// returns ip address of remote side of passed in remote ip
// returns 0 if remote ip unreachable
uint32_t tcp_node_get_local_ip(tcp_node_t tcp_node, uint32_t remote_ip);

/*********** for threading *********************************************/
void tcp_node_thread(tcp_node_t node, void *(*start_routine)(void*), struct tcp_api_args* args);
plain_list_t tcp_node_thread_list(tcp_node_t node);


/**** FOR TESTING *****/
uint32_t tcp_node_get_interface_remote_ip(tcp_node_t tcp_node, int interface_num);
uint32_t tcp_node_get_interface_local_ip(tcp_node_t tcp_node, int interface_num);


#endif //__TCP_NODE_H__
