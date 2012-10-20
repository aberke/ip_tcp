// stuct tcp_node owns a table described in WORKING_ON
#include <inttypes.h>
#include <netinet/ip.h>
#include <time.h>

#include "tcp_node.h"
#include "tcp_utils.h"
#include "tcp_connection.h"
#include "util/utils.h"

//// select
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

//// hash-map 
#include "uthash.h"


//// some helpful static globals
#define STDIN fileno(stdin)


struct tcp_node{

};