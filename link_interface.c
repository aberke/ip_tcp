#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util/list.h"
#include "link_interface.h"
#include "parselinks.h"

//wrapper around link_t structure they give us
// has read_packet function  -- reads from socket
// what it adds to link_t:
	sfd
	doesn't know its ip address
	boolean up_down  -- know whether or not it's down
	
has function send_packet() that takes in void* data  which is a packet constructed by node.c
	-- wraps udp protocol around this ip packet

has function read_packet() 
	returns NULL or ip_packet -- look into having this struct - might be in tools given
	
	
function get_sfd()
