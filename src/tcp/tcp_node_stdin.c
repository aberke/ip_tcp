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
#include <sys/time.h>

#include "util/utils.h"
#include "util/list.h"
#include "util/parselinks.h"
#include "tcp_node.h"
#include "tcp_connection.h"
#include "tcp_api.h"

#include "ip_node.h" 

#define FILE_BUF_SIZE	1024


void sockets_cmd(const char *line, tcp_node_t tcp_node){
	tcp_node_print(tcp_node);
}

void v_socket(const char *line, tcp_node_t tcp_node){
	int ret = tcp_api_socket(tcp_node);	
	printf("socket call returned value %d\n", ret);
}

void vv_bind(const char *line, tcp_node_t tcp_node){
	
	int socket;
	char* addr_str = malloc(sizeof(char)*INET_ADDRSTRLEN);
	int port;
	
	int ret = sscanf(line, "v_bind %d %s %d", &socket, addr_str, &port);
	if (ret != 3){
		fprintf(stderr, "syntax error (usage: v_bind [socket] [address] [port])\n");
		free(addr_str);
		return;
	} 	
	
		
	/* convert the given string into an address structure */
	struct in_addr* addr = malloc(sizeof(struct in_addr));
	inet_pton(AF_INET, addr_str, addr);
	free(addr_str);

	ret = tcp_api_bind(tcp_node, socket, addr, port);
	if(ret < 0)
		printf("v_bind returned error: %d\n", ret);
	else
		printf("v_bind returned: %d\n", ret);

	/* free it, hopefully tcp_api_bind didn't want to hang onto it */
	free(addr);
}

void v_listen(const char *line, tcp_node_t tcp_node){
	
	int socket;
	int ret = sscanf(line, "v_listen %d", &socket);
	if(ret !=1){
		fprintf(stderr, "syntax error (usage: v_listen [socket])\n");
		return;
	}
	ret = tcp_api_listen(tcp_node, socket);
	
	if(ret < 0)
		printf("Error: v_listen returned: %s\n", strerror(-ret));

	printf("v_listen returned %d\n", ret);
}

/*
accept/a port Open a socket, bind it to the given port, and start accepting connections on that
port. Your driver must continute to accept other commands.
*/

void accept_cmd(const char *line, tcp_node_t tcp_node){
	int ret, port, socket;
	
	ret = sscanf(line, "accept %d", &port);
	if((ret != 1)&&(sscanf(line, "a %d", &port)!=1)){
		fprintf(stderr, "syntax error (usage: accept [port])\n");
		return;
	}
	// create new socket
	if((socket = tcp_api_socket(tcp_node))<0){
		printf("Error: v_socket() returned: %s\n", strerror(-socket));
		return;
	}
	// now bind
	struct in_addr addr;	
	ret = tcp_api_bind(tcp_node, socket, &addr, port);
	if(ret < 0){
		printf("Error: v_bind returned: %s\n", strerror(-ret));
		return;
	}
	// now listen
	ret = tcp_api_listen(tcp_node, socket);	
	if(ret < 0){
		printf("Error: v_listen returned: %s\n", strerror(-ret));
		return;
	}
	// now accept in a loop - keep accepting	
	/* pack the args */
	tcp_api_args_t args = tcp_api_args_init();
	args->node		 = tcp_node;
	args->socket  	 = socket;
	args->function_call = "v_accept()";

	/* So we need to call tcp_api_accept in a loop without blocking.... so here's my solution that I've implemented:
		We call thread the call tcp_driver_accept_entry which then goes to call tcp_api_accept_entry in a loop.
		This means we're creating threads within the while loop thread, but this way we can pretty print the result
		of each individual tcp_api_accept call.  the thread for tcp_driver_accept_entry has return value 0
		because the actual accept calls that returned sockets will have already been printed.
	*/
	tcp_node_thread(tcp_node, tcp_driver_accept_entry, args);
	
	return;
}
void v_accept(const char *line, tcp_node_t tcp_node){
	/* this is for our personal api use of accept -- we just accept once rather than accepting on the 
		given socket forever in a loop like the accept_cmd.
		The distinction is made by the args->boolean parameter.
		if args->boolean = 0 (as in this case), then we just accept once
		else if args->boolean = 1: (as in accept_cmd case) then we accept in a while loop
		*/
	int ret, socket;
	
	ret = sscanf(line, "v_accept %d", &socket);
	if(ret != 1){
		fprintf(stderr, "syntax error (usage: v_accept [socket])\n");
		return;
	}
	
	//tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	struct in_addr* addr = malloc(sizeof(struct in_addr));
	
	/* pack the args */
	tcp_api_args_t args = tcp_api_args_init();
	args->node		 = tcp_node;
	args->socket  	 = socket;
	args->addr 		 = addr;
	args->function_call = "v_accept()";

	//just accept once, rather than in while loop like accept_cmd
	tcp_node_thread(tcp_node, tcp_api_accept_entry, args);

	return;
}

/*	connect/c ip port Attempt to connect to the given ip address, in dot notation, on the given port.
	Example: c 10.13.15.24 1056.
*/
void connect_cmd(const char *line, tcp_node_t tcp_node){

	struct in_addr* addr = malloc(sizeof(struct in_addr));
	char addr_buffer[INET_ADDRSTRLEN];
	int socket, port, ret;
	
	ret = sscanf(line, "connect %s %d", addr_buffer, &port);
	if((ret != 2)&&(sscanf(line, "c %s %d", addr_buffer, &port)!=2)){
		fprintf(stderr, "syntax error (usage: connect [remote ip address] [remote port])\n");
		free(addr);
		return;
	}	
	//convert string ip address to real ip address
	if(inet_pton(AF_INET, addr_buffer, addr) <= 0){ // IPv4
		fprintf(stderr, "syntax error - could not parse ip address (usage: connect [remote ip address] [remote port])\n");
		free(addr);
		return;
	}
	// first initialize new socket that will do the connecting
	if((socket = tcp_api_socket(tcp_node))<0){
		printf("Error: v_socket() returned value %d\n", socket);
		free(addr);
		return;
	}

	tcp_api_args_t args = tcp_api_args_init();
	args->node = tcp_node;
	args->socket = socket;
	args->addr = addr;
	args->port = port;
	args->function_call = "v_connect()";
	
	tcp_node_thread(tcp_node, tcp_api_connect_entry, args);
}
// allows us to call v_connect after calling our own v_socket rather than using driver connect_cmd
// expects: v_connect [socket] [ip address of remote connection] [remote port]
void v_connect(const char *line, tcp_node_t tcp_node){
	/*v_connect 0 10.10.168.73 13*/
	struct in_addr* addr = malloc(sizeof(struct in_addr));
	char addr_buffer[INET_ADDRSTRLEN];
	int socket, port, ret;
	
	ret = sscanf(line, "v_connect %d %s %d", &socket, addr_buffer, &port);
	if(ret != 3){
		fprintf(stderr, "syntax error (usage: v_connect [socket] [ip address] [port])\n");
		free(addr);
		return;
	}	
	//convert string ip address to real ip address
	if(inet_pton(AF_INET, addr_buffer, addr) <= 0){ // IPv4
		fprintf(stderr, "syntax error - could not parse ip address (usage: v_conect [socket] [ip address] [port])\n");
		free(addr);
		return;
	}

	tcp_api_args_t args = tcp_api_args_init();
	args->node = tcp_node;
	args->socket = socket;
	args->addr = addr;
	args->port = port;
	args->function_call = "v_connect()";
	
	tcp_node_thread(tcp_node, tcp_api_connect_entry, args);
}

/*shutdown socket read/write/both v shutdown on the given socket. If read or r is given, close
only the reading side. If write or w is given, close only the writing side. If both is given, close
both sides. Default is write*/
void shutdown_cmd(const char* line, tcp_node_t tcp_node){

	int socket;
	char r_w_both[BUFFER_SIZE];
	int ret;
	int r_w_option; // <-- we'll set args->num = r_w_option to convey if this is a v_shutdown(1/2/3)
	
	ret = sscanf(line, "shutdown %d %s", &socket, r_w_both);
	if(ret != 2){
		fprintf(stderr, "syntax error (usage: shutdown [socket] [read/write/both])\n");
		return;
	}
	if((!strcmp(r_w_both, "write")) || (!strcmp(r_w_both, "w"))){
		r_w_option = SHUTDOWN_WRITE;
	}
	else if((!strcmp(r_w_both, "read")) || (!strcmp(r_w_both, "r"))){
		r_w_option = SHUTDOWN_READ;
	}
	else if(!strcmp(r_w_both, "both")){
		r_w_option = SHUTDOWN_BOTH;
	}
	else{
		fprintf(stderr, "syntax error (usage: shutdown [socket] [read/write/both])\n");
		return;
	}
	tcp_api_args_t args = tcp_api_args_init();
	args->node = tcp_node;
	args->socket = socket;
	args->function_call = "v_shutdown";
	args->num = r_w_option;
		
	tcp_node_thread(tcp_node, tcp_api_shutdown_entry, args);
}
//close [socket]  :  v_close on the given socket.
void close_cmd(const char* line, tcp_node_t node){

	int socket, ret;
	
	ret = sscanf(line, "close %d", &socket);
	if(ret != 1){
		fprintf(stderr, "syntax error (usage: close [socket] )\n");
		return;
	}
	
	tcp_api_args_t args = tcp_api_args_init();
	args->node = node;
	args->socket = socket;
	args->function_call = "v_close";
		
	tcp_node_thread(node, tcp_api_close_entry, args);			
}
/* recvfile filename port 
	Listen for a connection on the given port. Once established, write every-
	thing you can read from the socket to the given file. Once the other side closes the connection,
	close the connection as well. Your driver must continue to accept other commands. */
void recvfile_cmd(const char* line, tcp_node_t tcp_node){
	int ret, socket, port;
	char* filename_buffer = (char*)malloc(sizeof(char)*FILE_BUF_SIZE);
	
	ret = sscanf(line, "recvfile %s %d", filename_buffer, &port);
	if(ret != 2){
		fprintf(stderr, "syntax error (usage: recvfile [filename] [port])\n");
		return;
	}	
	// create new socket
	if((socket = tcp_api_socket(tcp_node))<0){
		printf("Error: v_socket() returned: %s\n", strerror(-socket));
		free(filename_buffer);
		return;
	}
	// now bind
	struct in_addr addr;	
	ret = tcp_api_bind(tcp_node, socket, &addr, port);
	if(ret < 0){
		printf("Error: v_bind returned: %s\n", strerror(-ret));
		tcp_node_remove_connection_kernal(tcp_node, tcp_node_get_connection_by_socket(tcp_node, socket));
		free(filename_buffer);
		return;
	}
	// now listen
	ret = tcp_api_listen(tcp_node, socket);	
	if(ret < 0){
		printf("Error: v_listen returned: %s\n", strerror(-ret));
		free(filename_buffer);
		return;
	}
	// now accept and write	
	tcp_api_args_t args = tcp_api_args_init();
	args->node = tcp_node;
	args->socket = socket;
	args->buffer = filename_buffer;
	args->port = port;
	args->function_call = "recvfile()";
	
	tcp_node_thread(tcp_node, tcp_api_recvfile_entry, args);	
	return;
}
/*
recv/r socket numbytes y/n Try to read data from a given socket. If the last argument is y, then
you should block until numbytes is received, or the connection closes. If n, then don’t block;
return whatever recv returns. Default is n */
void recv_cmd(const char* line, tcp_node_t tcp_node){
	int socket;
	int num_bytes;
	char block_y_n[BUFFER_SIZE];
	int block = 0; //boolean as to whether recv should block or not
	int ret;
	
	ret = sscanf(line, "recv %d %d %s", &socket, &num_bytes, block_y_n);
	if(ret<2 || ret>3){
		// try again using 'r' instead of 'recv'
		ret = sscanf(line, "r %d %d %s", &socket, &num_bytes, block_y_n);
		if(ret<2 || ret>3){
			fprintf(stderr, "syntax error (usage: recv [socket] [numbytes] [y/n])\n");
			return;
		}
	}
	if(ret == 3){ // if ret == 2 then we keep block as false since Default is n
		if(!strcmp(block_y_n, "y"))
			block = 1;
		else if(strcmp(block_y_n, "n")){
			fprintf(stderr, "syntax error (usage: recv [socket] [numbytes] [y/n])\n");
			return;
		}	
	}
	if(num_bytes < 0){
		fprintf(stderr, "syntax error (usage: recv [socket] [numbytes] [y/n]) where numbytes is a positive integer\n");
		return;
	}
	tcp_api_args_t args = tcp_api_args_init();
	args->node = tcp_node;
	args->socket = socket;
	args->function_call = "v_read";
	args->boolean = 0;
	args->num = num_bytes;
		
	if(block)
		args->boolean = 1;
	tcp_node_thread(tcp_node, tcp_api_read_entry, args);
}
/* 
sendfile_cmd
	parses the given command into a socket and a file to send, 
	and then upon successful parsing of the arguments tries to
	open the file and then sends it off 
*/
void sendfile_cmd(const char* line, tcp_node_t tcp_node){
	int ret, socket, port;
	char* filename_buffer = (char*)malloc(sizeof(char)*FILE_BUF_SIZE);

	struct in_addr* addr = malloc(sizeof(struct in_addr));
	char addr_buffer[INET_ADDRSTRLEN];
	
	ret = sscanf(line, "sendfile %s %s %d", filename_buffer, addr_buffer, &port);
	if(ret != 3){
		fprintf(stderr, "syntax error (usage: sendfile [filename] [ip] [port])\n");
		free(filename_buffer);
		free(addr);
		return;
	}		
	//convert string ip address to real ip address
	if(inet_pton(AF_INET, addr_buffer, addr) <= 0){ // IPv4
		fprintf(stderr, "syntax error - could not parse ip address (usage: connect [remote ip address] [remote port])\n");
		free(filename_buffer);
		free(addr);
		return;
	}

	// first initialize new socket that will do the connecting
	if((socket = tcp_api_socket(tcp_node))<0){
		printf("Error: v_socket() returned value %d\n", socket);
		free(filename_buffer);
		free(addr);
		return;
	}

	tcp_api_args_t args = tcp_api_args_init();
	args->node = tcp_node;
	args->socket = socket;
	args->addr = addr;
	args->buffer = filename_buffer;
	args->port = port;
	args->function_call = "sendfile()";
	
	tcp_node_thread(tcp_node, tcp_api_sendfile_entry, args);	
	return;
}
/*send/s/w socket data Send a string on a socket. */
void send_cmd(const char* line, tcp_node_t tcp_node){

	int num_consumed;
	int socket;
	const char *data;
	int ret;
	
	ret = sscanf(line, "send %d %n", &socket, &num_consumed);
	if((ret != 1)&&(sscanf(line, "s %d %n", &socket, &num_consumed)!=1)&&(sscanf(line, "w %d %n", &socket, &num_consumed)!=1)){
		fprintf(stderr, "syntax error (usage: send/s/w [socket] [data])\n");
		return;
	} 
	data = line + num_consumed;
	if (strlen(data) < 2){ // 1-char message, plus newline
		fprintf(stderr, "syntax error (payload unspecified)\n");
		return;
	}
	
	ret = tcp_api_write(tcp_node, socket, (const unsigned char*)data, strlen(data)-1); // strlen()-1: stripping newline
	if (ret < 0){
		fprintf(stderr, "v_write() error: %s\n", strerror(-ret));
		return;
	}
	printf("v_write() on %d bytes returned %d\n", (int)(strlen(data)-1), ret);
}

void vv_write(const char* line, tcp_node_t tcp_node){
	int socket;
	char* to_write = malloc(sizeof(char)*BUFFER_SIZE);
	uint32_t num_bytes;
	
	if(sscanf(line, "v_write %d %s %u", &socket, to_write, &num_bytes) != 3){
		fprintf(stderr, "syntax error (usage: v_write [socket] [to_write] [bytes])\n");
		free(to_write);
		return;
	}

	int ret = tcp_api_write(tcp_node, socket, (unsigned char*)to_write, num_bytes);
	printf("v_write returned value: %d\n", ret);

	free(to_write);
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

void quit_cmd(const char *line, tcp_node_t tcp_node){
	// first must ABORT all connections
	tcp_node_ABORT_connections(tcp_node);

	tcp_node_stop(tcp_node);
	return;
}

void fp_cmd(const char *line, tcp_node_t tcp_node){
	tcp_node_command_ip(tcp_node, line);	
	return;
}

void rp_cmd(const char *line, tcp_node_t tcp_node){
	tcp_node_command_ip(tcp_node, line);	
	return;
}

void interfaces_cmd(const char *line, tcp_node_t tcp_node){
	tcp_node_command_ip(tcp_node, line);	
  	return;
}

void routes_cmd(const char *line, tcp_node_t tcp_node){
	tcp_node_command_ip(tcp_node, line);	
  	return;
}


void down_cmd(const char *line, tcp_node_t tcp_node){
	tcp_node_command_ip(tcp_node, line);	
  	return;
}

void up_cmd(const char *line, tcp_node_t tcp_node){
	tcp_node_command_ip(tcp_node, line);	
  	return;
}


void command_1(const char *line, tcp_node_t tcp_node){
	char cmd[256];
	strcpy(cmd, "v_connect 0 10.10.168.73 13");
	v_connect(cmd, tcp_node);
}

void command_2(const char* line, tcp_node_t tcp_node){
	char cmd[256];
	strcpy(cmd, "v_bind 0 1.1.1.1 13");
	vv_bind(cmd, tcp_node);
}

void command_3(const char* line, tcp_node_t tcp_node){
	char cmd[256];
	strcpy(cmd, "v_listen 0 5");
	v_listen(cmd, tcp_node);
}

void command_4(const char* line, tcp_node_t tcp_node){
	char cmd[256];
	strcpy(cmd, "v_write 0 hello 5");
	vv_write(cmd, tcp_node);
}

/* 
command:
	numbers <socket> <range>
		- sends all numbers over that established socket
		  ie numbers 1 10 would send the string 12345678910
		  and so on. string buffer is statically defined
		  to be 4096 bytes, so probably nothing over 1000
		  is a good idea (unless you like to live on the 
		  wild side)
*/
void numbers(const char* line, tcp_node_t tcp_node){
	int range, sock;
	if(sscanf(line, "numbers %d %d", &sock, &range) != 2){
		fprintf(stderr, "Syntax error: usage (numbers <socket> <number>)\n");
		return;
	}

	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, sock);
	if(!connection){
		printf("Error: %s\n", strerror(EBADF));
		return;
	}

	/* hope you don't go over that */
	char to_write[4096];
	memset(to_write, 0, 4096);
	
	int i;
	char int_string_buffer[BUFFER_SIZE];
	for(i=0;i<range;i++){
		sprintf(int_string_buffer, "%d", i);
		strcat(to_write,int_string_buffer);
	}
	strcat(to_write,"\n");
	
	tcp_connection_send_data(connection, (unsigned char*)to_write, strlen(to_write));
	// wanna do anything with that?
	
	return;//nah
}

/* Driver specs: window socket Print the socket’s send / receive window size */
void window_cmd(const char* line, tcp_node_t tcp_node){
	int socket;
	if(sscanf(line, "window %d", &socket) != 1){
		fprintf(stderr, "Syntax error: usage (window <socket>)\n");
		return;
	}

	tcp_connection_t connection = tcp_node_get_connection_by_socket(tcp_node, socket);
	if(!connection){
		printf("Error: %s\n", strerror(EBADF));
		return;
	}
	int s_size = 0;
	int r_size = 0;	
	send_window_t s = tcp_connection_get_send_window(connection);
	recv_window_t r = tcp_connection_get_recv_window(connection);

	if(s){
	 	s_size = send_window_get_size(s);
	}
	if(r){
		r_size = (int)recv_window_get_size(r);
	}

	printf("[socket %d]:\n\t send window size: %d\n\t receive window size: %d\n", socket, s_size, r_size);
}

struct {
  const char *command;
  void (*handler)(const char *, tcp_node_t);
} cmd_table[] = {
  {"help", help_cmd},
  {"interfaces", interfaces_cmd},
  {"li", interfaces_cmd},
  {"routes", routes_cmd},
  {"lr", routes_cmd},
  {"down", down_cmd},
  {"up", up_cmd},
  {"fp", fp_cmd},
  {"rp", rp_cmd},
  {"sockets", sockets_cmd}, 
  {"ls", sockets_cmd}, 
  {"window", window_cmd},
  {"recv", recv_cmd}, // calls tcp_api_read
  {"r", recv_cmd},	// calls tcp_api_read
  {"send", send_cmd},
  {"s", send_cmd},
  {"w", send_cmd},
  {"sendfile", sendfile_cmd},
  {"shutdown", shutdown_cmd},

  {"recvfile", recvfile_cmd},
  {"close", close_cmd},
  {"quit", quit_cmd},	// last two quit commands added by alex -- is this how we want to deal with quitting?
  {"q", quit_cmd},
  /*Also to directly test our api: */
  {"v_socket", v_socket}, // calls v_socket
  {"v_bind", vv_bind}, // calls v_bind
  {"v_listen", v_listen}, // calls v_listen
  {"v_connect", v_connect}, // calls v_connect
  {"connect", connect_cmd},
  {"c", connect_cmd},
  {"v_accept", v_accept}, // calls v_accept
  {"accept", accept_cmd}, // follows specs for driver -- opens socket, binds, , listens and starts accepting connections
  {"a", accept_cmd}, // follows specs for driver -- opens socket, binds, , listens and starts accepting connections

  {"v_write", vv_write},  // calls v_write
	
  // custom commands 
  {"1", command_1}, // performs command 'v_connect 0 10.10.168.73 12'
  {"2", command_2},
  {"3", command_3},
  {"4", command_4},
  {"numbers", numbers}
};


/* Thread for tcp_node to accept standard input and transfer that standard input to queue for tcp_node or ip_node to handle */
void* _handle_tcp_node_stdin(void* node){

	tcp_node_t tcp_node = (tcp_node_t)node;
	
	char line[LINE_MAX];
	char cmd[LINE_MAX];

	fd_set input;
	int ret, fgets_ret;
	unsigned i; 	

	/* let's just wait for 1/5 of a second for every stdin read */
	struct timeval tv;
	tv.tv_sec  = 1;
	tv.tv_usec = 0; 

	/* holder for the args that we're iterating over */
	tcp_api_args_t args;
	plain_list_el_t el;

	while (tcp_node_running(tcp_node)&&tcp_node_ip_running(tcp_node)){
		/* NOTE: this call fd_fgets (src/util/utils.c) is now non-blocking sort-of, 
			it blocks for the amount of time defined in the struct timeval tv, 
			and then returns control. It returns -1 if an error occurred, 0
			if there's nothing to read, and 1 if there's something to read */
		
		/* if less than 0, fgets() returned NULL */
		if( (fgets_ret = fd_fgets(&input, line, sizeof(line), stdin, &tv)) < 0 ){
			quit_cmd(line, tcp_node);
			break;	
		}
		else{
		/* >>>>>>>>>>>>>>>>> check all your spawned threads <<<<<<<<<< */
			/* this is defined in include/utils/list.h and it just
				iterates over the passed in list, setting the 
				given argument (thread) to the data of each 
				list element */

			plain_list_t list = tcp_node_thread_list(tcp_node);
			PLAIN_LIST_ITER(list, el) //in utils.h
				args = (tcp_api_args_t)el->data;
				if(args->done){
					if(args->result < 0){	
						char* error_string = strerror(-(args->result));
						printf("Error: %s returned %s\n", args->function_call, error_string);
					}
					else if(args->result==0)
						printf("%s on socket %d successful.\n", args->function_call, args->socket);
					
					else
						printf("%s on socket %d returned value: %d\n", args->function_call, args->socket, args->result);
					tcp_api_args_destroy(&args);
					plain_list_remove(list, el);
				}			
			PLAIN_LIST_ITER_DONE(list);

		/* >>>>>>>>>>>>> now check what you read on stdin <<<<<<<<<<< */

			/* didn't read anything */
			if(fgets_ret == 0)
				continue;
			/* otherwise line has been set by fgets in fd_fgets() */
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
	}
	print(("exiting stdin thread"), CLOSING_PRINT);
	pthread_exit(NULL);
}

