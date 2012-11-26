//tcp api
#ifndef __TCP_API_H__ 
#define __TCP_API_H__

#include "tcp_node.h"
#include "tcp_connection.h"
#include "utils.h"

#define SHUTDOWN_READ 2
#define SHUTDOWN_WRITE 1
#define SHUTDOWN_BOTH 3

//forward declaration:
struct tcp_connection;

/* for passing in to the spawned threads */
struct tcp_api_args{

	struct tcp_node* node;
	struct tcp_connection* connection;
	int socket;
	struct in_addr* addr;
	uint16_t port;
	char* function_call; //to be used for pretty result printing.  eg, v_accept() 
	
	int num; //multipurpose number
	int boolean; //multipurpose boolean -- eg, for tcp_api_read_entry, if true, blocks until reads num bytes
	char* buffer; //to be used for reading/writing
	
	pthread_t thread;
	int result;
	int done;
};

typedef struct tcp_api_args* tcp_api_args_t; 
tcp_api_args_t tcp_api_args_init();
int tcp_api_args_destroy(tcp_api_args_t* args);

/* connects a socket to an address (active OPEN in the RFC)
returns 0 on success or a negative number on failure */
int tcp_api_connect(struct tcp_node* node, int socket, struct in_addr* addr, uint16_t port);
void* tcp_api_connect_entry(void* args);

/* write on an open socket (SEND in the RFC)
return num bytes written or negative number on failure */
int tcp_api_write(tcp_node_t tcp_node, int socket, const unsigned char* to_write, uint32_t num_bytes);

void* tcp_api_sendfile_entry(void* _args);

int tcp_api_socket(struct tcp_node* node);
/* binds a socket to a port
always bind to all interfaces - which means addr is unused.
returns 0 on success or negative number on failure */
int tcp_api_bind(struct tcp_node* tcp_node, int socket,  struct in_addr* addr, uint16_t port);
int tcp_api_listen(struct tcp_node* tcp_node, int socket);

/* accept a requested connection (behave like unix socket’s accept)
returns new socket handle on success or negative number on failure 
int v accept(int socket, struct in addr *node); */
int tcp_api_accept(struct tcp_node* tcp_node, int socket, struct in_addr *addr);
// Not for driver use -- just for our use when we only want to accept once
void* tcp_api_accept_entry(void* args);

void* tcp_api_recvfile_entry(void* _args);

/* read on an open socket (RECEIVE in the RFC)
return num bytes read or negative number on failure or 0 on eof */
//int v read(int socket, unsigned char *buf, uint32 t nbyte);
int tcp_api_read(tcp_node_t tcp_node, int socket, char *buffer, uint32_t nbyte);
void* tcp_api_read_entry(void* _args);

void* tcp_driver_accept_entry(void* args);


//////////////////////////////////////////////////////////////////////////////////////
/*********************************** CLOSING *****************************************/



/* shutdown an open socket. If type is 1, close the writing part of
the socket (CLOSE call in the RFC. This should send a FIN, etc.)
If 2 is speciﬁed, close the reading part (no equivalent in the RFC;
v read calls should just fail, and the window size should not grow any
more). If 3 is speciﬁed, do both. The socket is not invalidated.
returns 0 on success, or negative number on failure
If the writing part is closed, any data not yet ACKed should still be retransmitted. */
int tcp_api_shutdown(tcp_node_t node, int socket, int type);
void* tcp_api_shutdown_entry(void* _args);

/* Invalidate this socket, making the underlying connection inaccessible to
any of these API functions. If the writing part of the socket has not been
shutdown yet, then do so. The connection shouldn't be terminated, though;
any data not yet ACKed should still be retransmitted. */
int tcp_api_close(tcp_node_t tcp_node, int socket);
void* tcp_api_close_entry(void* _args);




#endif //__TCP_API_H__
