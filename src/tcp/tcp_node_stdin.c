#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include "util/utils.h"
#include "util/list.h"
#include "util/parselinks.h"
#include "tcp_node.h"
#include "tcp_connection.h"

#include "ip_node.h" 

#define FILE_BUF_SIZE	1024

#define SHUTDOWN_READ	0
#define SHUTDOWN_WRITE	1
#define SHUTDOWN_BOTH	2

// TODO remove the below #defines, replace by linking with API implementation 
#define v_connect(a,b,c)	-ENOTSUP
#define v_accept(a,b,c)	-ENOTSUP
#define v_write(a,b,c)	-ENOTSUP
#define v_read(a,b,c)	-ENOTSUP
#define v_shutdown(a,b)	-ENOTSUP
#define v_close(a)	-ENOTSUP

void sockets_cmd(const char *line, tcp_node_t tcp_node){
	tcp_node_print(tcp_node);
}

int v_socket(tcp_node_t tcp_node){
	tcp_connection_t connection = tcp_node_new_connection(tcp_node);
	int socket = tcp_connection_get_socket(connection);
	return socket;
}
void vv_socket(const char *line, tcp_node_t tcp_node){
		int socket = v_socket(tcp_node);
		printf("socket: %d\n", socket);	
}
/* binds a socket to a port
always bind to all interfaces - which means addr is unused.
returns 0 on success or negative number on failure */
int v_bind(tcp_node_t tcp_node, int socket, char* addr, uint16_t port){

	// check if port already in use
	if(!tcp_node_port_unused(tcp_node, port))		
		return EADDRINUSE;	//The given address is already in use.

	// get corresponding tcp_connection
	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)
		return EBADF; 	//socket is not a valid descriptor

	if(tcp_connection_get_local_port(connection))
		return EINVAL; 	// The socket is already bound to an address.

	tcp_node_assign_port(tcp_node, connection, port);

	return 0;
}
void vv_bind(const char *line, tcp_node_t tcp_node){
	
	int socket;
	char* addr = malloc(sizeof(char)*FILE_BUF_SIZE);
	int port;
	
	int ret = sscanf(line, "v_bind %d %s %d", &socket, addr, &port);
	if (ret != 3){
		fprintf(stderr, "syntax error (usage: v_bind [socket] [address] [port])\n");
		free(addr);
		return;
	} 	
	ret = v_bind(tcp_node, socket, addr, port);
	printf("bind result: %d\n", ret);
	free(addr);
}
// returns port that connection is listening on, negative number on failure
int v_listen(tcp_node_t tcp_node, int socket){

	int port;

	// get corresponding tcp_connection
	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(connection == NULL)
		return EBADF; 	//socket is not a valid descriptor

	if(!tcp_connection_get_local_port(connection)){
		// port not already set -- must bind to random port	
		port = tcp_node_next_port(tcp_node);
		tcp_node_assign_port(tcp_node, connection, port);
	}
	
	if(tcp_connection_passive_open(connection) < 0){ // returns -1 on failure
		return -1;
	}
	
	port = (int)tcp_connection_get_local_port(connection);
	
	return port; // returns 0 on success
}

void vv_listen(const char *line, tcp_node_t tcp_node){
	
	int socket;
	int ret = sscanf(line, "v_listen %d", &socket);
	if(ret !=1){
		fprintf(stderr, "syntax error (usage: v_listen [socket])\n");
		return;
	}
	ret = v_listen(tcp_node, socket);
	printf("listen result: %d\n", ret);
}
void vv_accept(const char *line, tcp_node_t tcp_node){

}
void vv_connect(const char *line, tcp_node_t tcp_node){

}
/*
struct sendrecvfile_arg {
  int s;
  int fd;
};

int v_write_all(int s, const void *buf, size_t bytes_requested){
  int ret;
  size_t bytes_written;

  bytes_written = 0;
  while (bytes_written < bytes_requested){
    ret = v_write(s, buf + bytes_written, bytes_requested - bytes_written);
    if (ret == -EAGAIN){
      continue;
    }
    if (ret < 0){
      return ret;
    }
    if (ret == 0){
      fprintf(stderr, "warning: v_write() returned 0 before all bytes written\n");
      return bytes_written;
    }
    bytes_written += ret;
  }

  return bytes_written;
}

int v_read_all(int s, void *buf, size_t bytes_requested){
  int ret;
  size_t bytes_read;

  bytes_read = 0;
  while (bytes_read < bytes_requested){
    ret = v_read(s, buf + bytes_read, bytes_requested - bytes_read);
    if (ret == -EAGAIN){
      continue;
    }
    if (ret < 0){
      return ret;
    }
    if (ret == 0){
      fprintf(stderr, "warning: v_read() returned 0 before all bytes read\n");
      return bytes_read;
    }
    bytes_read += ret;
  }

  return bytes_read;
}
*/
void help_cmd(const char *line, tcp_node_t tcp_node){
  (void)line;

  printf("- help: Print this list of commands.\n"
         "- interfaces: Print information about each interface, one per line.\n"
         "- routes: Print information about the route to each known destination, one per line.\n"
         "- sockets: List all sockets, along with the state the TCP connection associated with them is in, and their current window sizes.\n"
         "- down [integer]: Bring an interface \"down\".\n"
         "- up [integer]: Bring an interface \"up\" (it must be an existing interface, probably one you brought down)\n"
         "- accept [port]: Spawn a socket, bind it to the given port, and start accepting connections on that port.\n"
         "- connect [ip] [port]: Attempt to connect to the given ip address, in dot notation, on the given port.  send [socket] [data]: Send a string on a socket.\n"
         "- recv [socket] [numbytes] [y/n]: Try to read data from a given socket. If the last argument is y, then you should block until numbytes is received, or the connection closes. If n, then don.t block; return whatever recv returns. Default is n.\n"
         "- sendfile [filename] [ip] [port]: Connect to the given ip and port, send the entirety of the specified file, and close the connection.\n"
         "- recvfile [filename] [port]: Listen for a connection on the given port. Once established, write everything you can read from the socket to the given file. Once the other side closes the connection, close the connection as well.\n"
         "- shutdown [socket] [read/write/both]: v_shutdown on the given socket. If read is given, close only the reading side. If write is given, close only the writing side. If both is given, close both sides. Default is write.\n"
         "- close [socket]: v_close on the given socket.\n");

  return;
}

// puts command on to stdin_commands queue for ip_node to handle
void ip_handle_cmd(const char *line, tcp_node_t tcp_node){
	char* cmd_item = (char *)malloc(sizeof(char)*LINE_MAX);
	strcpy(cmd_item, line);

	tcp_node_queue_ip_cmd(tcp_node, cmd_item);
	return;	
}

void quit_cmd(const char *line, tcp_node_t tcp_node){

	ip_handle_cmd(line, tcp_node);	
	return;
}
void fp_cmd(const char *line, tcp_node_t tcp_node){

	ip_handle_cmd(line, tcp_node);	
	return;
}
void rp_cmd(const char *line, tcp_node_t tcp_node){

	ip_handle_cmd(line, tcp_node);	
	return;
}
void interfaces_cmd(const char *line, tcp_node_t tcp_node){

	ip_handle_cmd(line, tcp_node);	
  	return;
}

void routes_cmd(const char *line, tcp_node_t tcp_node){

	ip_handle_cmd(line, tcp_node);	
  	return;
}


void down_cmd(const char *line, tcp_node_t tcp_node){

	ip_handle_cmd(line, tcp_node);	
  	return;
}

void up_cmd(const char *line, tcp_node_t tcp_node){

	ip_handle_cmd(line, tcp_node);	
  	return;
}



void send_cmd(const char *line, tcp_node_t tcp_node){
  /*typedef struct tcp_packet_data{
	uint32_t local_virt_ip;
	uint32_t remote_virt_ip;
	char packet[MTU];
	int packet_size;  //size of packet in bytes
} tcp_packet_data_t;*/

  
  /* TODO: ONCE HAVE WORKING API MAKE SURE TO INSTEAD IMPLEMENT BELOW VERSION */
	/*  
	int num_consumed;
	int socket;
	const char *data;
	int ret;
	
	ret = sscanf(line, "send %d %n", &socket, &num_consumed);
	if (ret != 1){
		fprintf(stderr, "syntax error (usage: send [interface] [payload])\n");
		return;
	} 
	data = line + num_consumed;
	if (strlen(data) < 2){ // 1-char message, plus newline
		fprintf(stderr, "syntax error (payload unspecified)\n");
		return;
	}
	
	ret = v_write(socket, data, strlen(data)-1); // strlen()-1: stripping newline
	if (ret < 0){
		fprintf(stderr, "v_write() error: %s\n", strerror(-ret));
		return;
	}
	printf("v_write() on %d bytes returned %d\n", strlen(data)-1, ret);
*/
  return;
}
/*
void *accept_thr_func(void *arg){
  int s;
  int ret;

  s = (int)arg;

  while (1){
    ret = v_accept(s, NULL, NULL);
    if (ret < 0){
      fprintf(stderr, "v_accept() error on socket %d: %s\n", s, strerror(-ret));
      return NULL;
    }
    printf("v_accept() on socket %d returned %d\n", s, ret);
  }

  return NULL;
}

void accept_cmd(const char *line, tcp_node_t tcp_node){
  uint16_t port;
  int ret;
  struct in_addr any_addr;
  int s;
  pthread_t accept_thr;
  pthread_attr_t thr_attr;

  ret = sscanf(line, "accept %" SCNu16, &port);
  if (ret != 1){
    fprintf(stderr, "syntax error (usage: accept [port])\n");
    return;
  }

  s = v_socket();
  if (s < 0){
    fprintf(stderr, "v_socket() error: %s\n", strerror(-s));
    return;
  }
  any_addr.s_addr = 0;
  ret = v_bind(s, any_addr, htons(port));
  if (ret < 0){
    fprintf(stderr, "v_bind() error: %s\n", strerror(-ret));
    return;
  }
  ret = v_listen(s);
  if (ret < 0){
    fprintf(stderr, "v_listen() error: %s\n", strerror(-ret));
    return;
  }
  ret = pthread_attr_init(&thr_attr);
  assert(ret == 0);
  ret = pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED);
  assert(ret == 0);
  ret = pthread_create(&accept_thr, &thr_attr, accept_thr_func, (void *)s);
  if (ret != 0){
    fprintf(stderr, "pthread_create() error: %s\n", strerror(errno));
    return;
  }
  ret = pthread_attr_destroy(&thr_attr);
  assert(ret == 0);

  return;
}

void connect_cmd(const char *line, tcp_node_t tcp_node){
  char ip_string[LINE_MAX];
  struct in_addr ip_addr;
  uint16_t port;
  int ret;
  int s;
  
  ret = sscanf(line, "connect %s %" SCNu16, ip_string, &port);
  if (ret != 2){
    fprintf(stderr, "syntax error (usage: connect [ip address] [port])\n");
    return;
  }
  ret = inet_aton(ip_string, &ip_addr);
  if (ret == 0){
    fprintf(stderr, "syntax error (malformed ip address)\n");
    return;
  }

  s = v_socket();
  if (s < 0){
    fprintf(stderr, "v_socket() error: %s\n", strerror(-s));
    return;
  }
  ret = v_connect(s, ip_addr, htons(port));
  if (ret < 0){
    fprintf(stderr, "v_connect() error: %s\n", strerror(-ret));
    return;
  }
  printf("v_connect() returned %d\n", ret);

  return;
}


void recv_cmd(const char *line, tcp_node_t tcp_node){
  int socket;
  size_t bytes_requested;
  int bytes_read;
  char should_loop;
  char *buffer;
  int ret;
  
  ret = sscanf(line, "recv %d %zu %c", &socket, &bytes_requested, &should_loop);
  if (ret != 3){
    should_loop = 'n';
    ret = sscanf(line, "recv %d %zu", &socket, &bytes_requested);
    if (ret != 2){
      fprintf(stderr, "syntax error (usage: recv [interface] [bytes to read] "
                                                "[loop? (y/n), optional])\n");
      return;
    }
  }

  buffer = (char *)malloc(bytes_requested+1); // extra for null terminator
  assert(buffer);
  memset(buffer, '\0', bytes_requested+1);
  if (should_loop == 'y'){
    bytes_read = v_read_all(socket, buffer, bytes_requested);
  }
  else if (should_loop == 'n'){
    bytes_read = v_read(socket, buffer, bytes_requested);
  }
  else {
    fprintf(stderr, "syntax error (loop option must be 'y' or 'n')\n");
    goto cleanup;
  }

  if (bytes_read < 0){
    fprintf(stderr, "v_read() error: %s\n", strerror(-bytes_read));
    goto cleanup;
  }
  buffer[bytes_read] = '\0';
  printf("v_read() on %zu bytes returned %d; contents of buffer: '%s'\n",
         bytes_requested, bytes_read, buffer);

cleanup:
  free(buffer);
  return;
}

void *sendfile_thr_func(void *arg){
  int s;
  int fd;
  int ret;
  struct sendrecvfile_arg *thr_arg;
  int bytes_read;
  char buf[FILE_BUF_SIZE];

  thr_arg = (struct sendrecvfile_arg *)arg;
  s = thr_arg->s;
  fd = thr_arg->fd;
  free(thr_arg);

  while ((bytes_read = read(fd, buf, sizeof(buf))) != 0){
    if (bytes_read == -1){
      fprintf(stderr, "read() error: %s\n", strerror(errno));
      break;
    }
    ret = v_write_all(s, buf, bytes_read);
    if (ret < 0){
      fprintf(stderr, "v_write() error: %s\n", strerror(-ret));
      break;
    }
    if (ret != bytes_read){
      break;
    }
  }

  ret = v_close(s);
  if (ret < 0){
    fprintf(stderr, "v_close() error: %s\n", strerror(-ret));
  }
  ret = close(fd);
  if (ret == -1){
    fprintf(stderr, "close() error: %s\n", strerror(errno));
  }

  printf("sendfile on socket %d done\n", s);
  return NULL;
}

void sendfile_cmd(const char *line, tcp_node_t tcp_node){
  int ret;
  char filename[LINE_MAX];
  char ip_string[LINE_MAX];
  struct in_addr ip_addr;
  uint16_t port; 
  int s;
  int fd;
  struct sendrecvfile_arg *thr_arg;
  pthread_t sendfile_thr;
  pthread_attr_t thr_attr;

  ret = sscanf(line, "sendfile %s %s %" SCNu16, filename, ip_string, &port);
  if (ret != 3){
    fprintf(stderr, "syntax error (usage: sendfile [filename] [ip address]"
                                                  "[port])\n");
    return;
  }
  ret = inet_aton(ip_string, &ip_addr);
  if (ret == 0){
    fprintf(stderr, "syntax error (malformed ip address)\n");
    return;
  }

  s = v_socket();
  if (s < 0){
    fprintf(stderr, "v_socket() error: %s\n", strerror(-s));
    return;
  }
  ret = v_connect(s, ip_addr, htons(port));
  if (ret < 0){
    fprintf(stderr, "v_connect() error: %s\n", strerror(-ret));
    return;
  }
  fd = open(filename, O_RDONLY);
  if (fd == -1){
    fprintf(stderr, "open() error: %s\n", strerror(errno));
  }
  thr_arg = (struct sendrecvfile_arg *)malloc(sizeof(struct sendrecvfile_arg));
  assert(thr_arg);
  thr_arg->s = s;
  thr_arg->fd = fd;
  ret = pthread_attr_init(&thr_attr);
  assert(ret == 0);
  ret = pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED);
  assert(ret == 0);
  ret = pthread_create(&sendfile_thr, &thr_attr, sendfile_thr_func, thr_arg);
  if (ret != 0){
    fprintf(stderr, "pthread_create() error: %s\n", strerror(errno));
    return;
  }
  ret = pthread_attr_destroy(&thr_attr);
  assert(ret == 0);

  return;
}

void *recvfile_thr_func(void *arg){
  int s;
  int s_data;
  int fd;
  int ret;
  struct sendrecvfile_arg *thr_arg;
  int bytes_read;
  char buf[FILE_BUF_SIZE];

  thr_arg = (struct sendrecvfile_arg *)arg;
  s = thr_arg->s;
  fd = thr_arg->fd;
  free(thr_arg);

  s_data = v_accept(s, NULL, NULL);
  if (s_data < 0){
    fprintf(stderr, "v_accept() error: %s\n", strerror(-s_data));
    return NULL;
  }
  ret = v_close(s);
  if (ret < 0){
    fprintf(stderr, "v_close() error: %s\n", strerror(-ret));
    return NULL;
  }

  while ((bytes_read = v_read(s_data, buf, FILE_BUF_SIZE)) != 0){
    if (bytes_read < 0){
      fprintf(stderr, "v_read() error: %s\n", strerror(-bytes_read));
      break;
    }
    ret = write(fd, buf, bytes_read);
    if (ret < 0){
      fprintf(stderr, "write() error: %s\n", strerror(errno));
      break;
    }
  }

  ret = v_close(s_data);
  if (ret < 0){
    fprintf(stderr, "v_close() error: %s\n", strerror(-ret));
  }
  ret = close(fd);
  if (ret == -1){
    fprintf(stderr, "close() error: %s\n", strerror(errno));
  }

  printf("recvfile on socket %d done", s_data);
  return NULL;
}

void recvfile_cmd(const char *line, tcp_node_t tcp_node){
  int ret;
  char filename[LINE_MAX];
  uint16_t port;
  int s;
  struct in_addr any_addr;
  pthread_t recvfile_thr;
  pthread_attr_t thr_attr;
  struct sendrecvfile_arg *thr_arg;
  int fd;

  ret = sscanf(line, "recvfile %s %" SCNu16, filename, &port);
  if (ret != 2){
    fprintf(stderr, "syntax error (usage: recvfile [filename] [port])\n");
    return;
  }

  s = v_socket();
  if (s < 0){
    fprintf(stderr, "v_socket() error: %s\n", strerror(-s));
    return;
  }
  any_addr.s_addr = 0;
  ret = v_bind(s, any_addr, htons(port));
  if (ret < 0){
    fprintf(stderr, "v_bind() error: %s\n", strerror(-ret));
    return;
  }
  ret = v_listen(s);
  if (ret < 0){
    fprintf(stderr, "v_listen() error: %s\n", strerror(-ret));
    return;
  }
  fd = open(filename, O_WRONLY | O_CREAT);
  if (fd == -1){
    fprintf(stderr, "open() error: %s\n", strerror(errno));
  }
  thr_arg = (struct sendrecvfile_arg *)malloc(sizeof(struct sendrecvfile_arg));
  assert(thr_arg);
  thr_arg->s = s;
  thr_arg->fd = fd;
  ret = pthread_attr_init(&thr_attr);
  assert(ret == 0);
  ret = pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED);
  assert(ret == 0);
  ret = pthread_create(&recvfile_thr, &thr_attr, recvfile_thr_func, thr_arg);
  if (ret != 0){
    fprintf(stderr, "pthread_create() error: %s\n", strerror(errno));
    return;
  }
  ret = pthread_attr_destroy(&thr_attr);
  assert(ret == 0);

  return;
}

void shutdown_cmd(const char *line, tcp_node_t tcp_node){
  char shut_type[LINE_MAX];
  int shut_type_int;
  int socket;
  int ret;

  ret = sscanf(line, "shutdown %d %s", &socket, shut_type);
  if (ret != 2){
    fprintf(stderr, "syntax error (usage: shutdown [socket] [shutdown type])\n");
    return;
  }

  if (!strcmp(shut_type, "read")){
    shut_type_int = SHUTDOWN_READ;
  }
  else if (!strcmp(shut_type, "write")){
    shut_type_int = SHUTDOWN_WRITE;
  }
  else if (!strcmp(shut_type, "both")){
    shut_type_int = SHUTDOWN_BOTH;
  }
  else {
    fprintf(stderr, "syntax error (type option must be 'read', 'write', or "
                    "'both')\n");
    return;
  }

  ret = v_shutdown(socket, shut_type_int);
  if (ret < 0){
    fprintf(stderr, "v_shutdown() error: %s\n", strerror(-ret)); 
    return;
  }

  printf("v_shutdown() returned %d\n", ret);
  return;
}

void close_cmd(const char *line, tcp_node_t tcp_node){
  int ret;
  int socket;

  ret = sscanf(line, "close %d", &socket);
  if (ret != 1){
    fprintf(stderr, "syntax error (usage: close [socket])\n");
    return;
  }

  ret = v_close(socket);
  if (ret < 0){
    fprintf(stderr, "v_close() error: %s\n", strerror(-ret));
    return;
  }

  printf("v_close() returned %d\n", ret);
  return;
}

void sockets_cmd(const char *line, tcp_node_t tcp_node){
  (void)line;

  printf("not yet implemented: sockets\n");
  // TODO print sockets

  return;
}

*/


struct {
  const char *command;
  void (*handler)(const char *, tcp_node_t);
} cmd_table[] = {
  {"help", help_cmd},
  {"send", send_cmd},
  {"interfaces", interfaces_cmd},
  {"routes", routes_cmd},
  {"down", down_cmd},
  {"up", up_cmd},
  {"fp", fp_cmd},
  {"rp", rp_cmd},
  {"sockets", sockets_cmd},/*
  {"accept", accept_cmd},
  {"connect", connect_cmd},
  {"recv", recv_cmd},
  {"sendfile", sendfile_cmd},
  {"recvfile", recvfile_cmd},
  {"shutdown", shutdown_cmd},
  {"close", close_cmd},*/
  {"quit", quit_cmd},	// last two quit commands added by alex -- is this how we want to deal with quitting?
  {"q", quit_cmd},
  /*Also to directly test our api: */
  {"v_socket", vv_socket}, // calls v_socket
  {"v_bind", vv_bind}, // calls v_bind
  {"v_listen", vv_listen}, // calls v_listen
  {"v_connect", vv_connect}, // calls v_connect
  {"v_accept", vv_accept} // calls v_accept
};


/* Thread for tcp_node to accept standard input and transfer that standard input to queue for tcp_node or ip_node to handle */
void* _handle_tcp_node_stdin(void* node){

	tcp_node_t tcp_node = (tcp_node_t)node;
	
	char line[LINE_MAX];
	char cmd[LINE_MAX];
	char *fgets_ret;
	int ret;
	unsigned i; 	
	
	while (tcp_node_running(tcp_node)&&tcp_node_ip_running(tcp_node)){
		
		fgets_ret = fgets(line, sizeof(line), stdin);
		if (fgets_ret == NULL){
			break;
		}
		
		ret = sscanf(line, "%s", cmd);
		if (ret != 1){
			fprintf(stderr, "syntax error (first argument must be a command)\n");
			continue;
		}
				
		for (i=0; i < sizeof(cmd_table) / sizeof(cmd_table[0]); i++){
			if (!strcmp(cmd, cmd_table[i].command)){
				cmd_table[i].handler(line, tcp_node);
				break;
			}
		}
		
		if (i == sizeof(cmd_table) / sizeof(cmd_table[0])){
			fprintf(stderr, "error: no valid command specified\n");
			continue;
		}
	}
	pthread_exit(NULL);
}

