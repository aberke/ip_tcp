// stuct tcp_node owns a table described in WORKING_ON
#include <inttypes.h>
#include <netinet/ip.h>
#include <time.h>

#include "tcp_utils.h"
#include "tcp_node_stdin.h"
#include "tcp_connection.h"
#include "util/utils.h"
#include "util/list.h"
#include "util/parselinks.h"

//#include "bqueue.h" -- defined in header ip_node.h
#include "ip_node.h"
#include "tcp_node.h"

//// select
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

//// hash-map 
#include "uthash.h"



// static functions
static void _handle_read(tcp_node_t tcp_node);
static int _start_ip_threads(tcp_node_t tcp_node, 
			pthread_t ip_link_interface_thread, pthread_t ip_send_thread, pthread_t ip_command_thread);
static int _start_stdin_thread(tcp_node_t tcp_node, pthread_t tcp_stdin_thread);



struct tcp_node{
	int running; // 0 for not running, 1 for running // mimics ip_node
	ip_node_t ip_node;	// owns an ip_node_t that it keeps running to send/receive/listen on its interfaces
	bqueue_t *to_send;	//--- tcp data for ip to send
	bqueue_t *to_read;	//--- tcp data that ip pushes on to queue for tcp to handle
	bqueue_t *stdin_commands;	//---  way for tcp_node to pass user input commands to ip_node
	// owns statemachine
	// owns window
	// owns table of tcp_connections
};


/* helper function to tcp_node_start -- does the work of starting up _handle_tcp_node_stdin() in a thread */
static int _start_stdin_thread(tcp_node_t tcp_node, pthread_t tcp_stdin_thread){		
	
	/* Initialize and set thread detached attribute */
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		
	int status = pthread_create(&tcp_stdin_thread, &attr, _handle_tcp_node_stdin, (void *)tcp_node);
    if (status){
         printf("ERROR; return code from pthread_create() for _handle_tcp_node_stdin is %d\n", status);
         return 0;
    }
    puts("started tcp_stdin_thread");
    pthread_attr_destroy(&attr);
	return 1;
}

/* helper function to tcp_node_start -- does the work of starting up ip_node_start() in a thread */
static int _start_ip_threads(tcp_node_t tcp_node, 
			pthread_t ip_link_interface_thread, pthread_t ip_send_thread, pthread_t ip_command_thread){
	/*struct ip_thread_data{
		ip_node_t ip_node;
		bqueue_t *to_send;
		bqueue_t *to_read;
		bqueue_t *stdin_commands;   // way for tcp_node to pass user input commands to ip_node
	};*/
	// fetch and put arguments into ip_thread_data_t
	ip_thread_data_t ip_data = (ip_thread_data_t)malloc(sizeof(struct ip_thread_data));
	ip_data->ip_node = tcp_node->ip_node;
	ip_data->to_send = tcp_node->to_send;
	ip_data->to_read = tcp_node->to_read;
	ip_data->stdin_commands = tcp_node->stdin_commands;	
	
	/* Initialize and set thread detached attribute */
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	// start up each thread	
	int status;
	status = pthread_create(&ip_link_interface_thread, &attr, ip_link_interface_thread_run, (void *)ip_data);
    if (status){
         printf("ERROR; return code from pthread_create() for ip_link_interface_thread is %d\n", status);
         free(ip_data);
         return 0;
    }
    puts("started ip_thread: link_interface_thread");
    status = pthread_create(&ip_send_thread, NULL, ip_send_thread_run, (void *)ip_data);
    if (status){
         printf("ERROR; return code from pthread_create() for ip_send_thread is %d\n", status);
         free(ip_data);
         return 0;
    }
    puts("started ip_thread: ip_send_thread");
    status = pthread_create(&ip_command_thread, NULL, ip_command_thread_run, (void *)ip_data);
    if (status){
         printf("ERROR; return code from pthread_create() for ip_command_thread is %d\n", status);
         free(ip_data);
         return 0;
    }
    puts("started ip_thread: ip_command_thread");
    pthread_attr_destroy(&attr);
    //free(ip_data);  -- free called in ip_command_thread_run
	return 1;
}

tcp_node_t tcp_node_init(iplist_t* links){
	// initalize ip_node -- return FAILURE if ip_node a failure
	ip_node_t ip_node = ip_node_init(links);
	if(!ip_node)
		return NULL;

	// create tcp_node
	tcp_node_t tcp_node = (tcp_node_t)malloc(sizeof(struct tcp_node));	
	tcp_node->ip_node = ip_node;
				
	// create to_send and to_read stdin_commands queues that allow tcp_node and ip_node to communicate
	// to_send is loaded with tcp packets by tcp_node that need to be sent by ip_node
	bqueue_t *to_send = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(to_send);
	tcp_node->to_send = to_send;
	// to_read is loaded with unwrapped ip_packets by ip_node that tcp_node needs to handle and unwrap as tcp packets
	bqueue_t *to_read = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(to_read);
	tcp_node->to_read = to_read;
	// stdin_commands is for the tcp_node to pass user input commands to ip_node that ip_node needs to handle
	bqueue_t *stdin_commands = (bqueue_t*) malloc(sizeof(bqueue_t));   // way for tcp_node to pass user input commands to ip_node
	bqueue_init(stdin_commands);	
	tcp_node->stdin_commands = stdin_commands;
	
	// init statemachine
	// init window
	
	// create kernal table
	
	//// you're still running right? right
	tcp_node->running = 1;
	
	return tcp_node;
}

void tcp_node_destroy(tcp_node_t tcp_node){
	// destroy ip_node
	ip_node_t ip_node = tcp_node->ip_node;
	ip_node_destroy(&ip_node);
	// destroy bqueues
	puts("About to destroy queues");
	bqueue_destroy(tcp_node->to_send);
	bqueue_destroy(tcp_node->to_read);
	bqueue_destroy(tcp_node->stdin_commands);
	
	// TODO: REST OF CLEAN UP
	
	//destroy statemachine
	// destroy window
	
	free(tcp_node);
	tcp_node = NULL;
	puts("tcp_node destroyed");
}
void tcp_node_print(tcp_node_t tcp_node);

void tcp_node_start(tcp_node_t tcp_node){
	/* tcp_node runs 4 threads: 
		pthread_t tcp_stdin_thread to catch	stdin and put commands in stdin_queue for tcp_node to handle
		ip_link_interface_thread:  reads off link interfaces (current select() loop) and throws packets for tcp to handle into to_read
		ip_send_thread: calls p_thread_cond_wait(&to_send) and sends things loaded on to queue
		ip_command_thread: calls p_thread_cond_wait(&stdin_commands) and handles ip commands loaded on to queue
	*/
	pthread_t tcp_stdin_thread, ip_link_interface_thread, ip_send_thread, ip_command_thread;
	
	// start up ip_node threads
	if(!_start_ip_threads(tcp_node, ip_link_interface_thread, ip_send_thread, ip_command_thread)){
		// failed to start thread that runs ip_node_start() -- get out and destroy
		puts("Failed to start ip_node");
		return;
	}

	// start thread for reading in standard inputs
	if(!_start_stdin_thread(tcp_node, tcp_stdin_thread)){
		// failed to start thread that runs tcp_node_stdin_thread() -- get out and destroy
		puts("Failed to start tcp_node_stdin_thread");
		return;
	}
		
	// create timespec for timeout on pthread_cond_timedwait(&to_read);
	struct timespec wait_cond = {PTHREAD_COND_TIMEOUT_SEC, PTHREAD_COND_TIMEOUT_NSEC}; //
	int wait_cond_ret;
	
	while((tcp_node->running)&&(ip_node_running(tcp_node->ip_node))){	
		bqueue_t *to_read = tcp_node->to_read;

		if(!(bqueue_empty(to_read))){
			// handle reading tcp packet from ip_node
			_handle_read(tcp_node);	
		}
		else{
			// wait a moment for to_read to fill -- or continue through while loop after a moment passes
			pthread_mutex_lock(&(to_read->q_mtx));
        	if((wait_cond_ret = pthread_cond_timedwait(&(to_read->q_cond), &(to_read->q_mtx), &wait_cond))!=0){
        		if(wait_cond_ret == ETIMEDOUT){
        			// timed out
      				//puts("pthread_cond_timed_wait for to_read timed out");
      			}
      			else{
      				printf("ERROR: pthread_cond_timed_wait errored out\n");
      			}
      			// unlock and continue
      			pthread_mutex_unlock(&(to_read->q_mtx));
      			continue;
      		}
			pthread_mutex_unlock(&(to_read->q_mtx));
			_handle_read(tcp_node);
		}
	}
	int rc;
	// is my segmentation fault issue in the threads?
	puts("joining thread 1");
	rc = pthread_join(ip_link_interface_thread, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_join() is %d\n", rc);
		exit(-1);
	}
	puts("joining thread 2");
	rc = pthread_join(ip_send_thread, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_join() is %d\n", rc);
		exit(-1);
	}
	puts("joining thread 3");
	rc = pthread_join(ip_command_thread, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_join() is %d\n", rc);
		exit(-1);
	}
	puts("joining thread 4");
	rc = pthread_join(tcp_stdin_thread, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_join() is %d\n", rc);
		exit(-1);
	}
	puts("threads joined");
}
// puts command on to stdin_commands queue
// returns 1 on success, -1 on failure (failure when queue actually already destroyed)
int tcp_node_queue_ip_cmd(tcp_node_t tcp_node, char* buffered_cmd){
	
	bqueue_t *stdin_commands = tcp_node->stdin_commands;
	
	if(bqueue_enqueue(stdin_commands, (void*)buffered_cmd))
		return -1;
	
	return 1;
}
// returns tcp_node->running
int tcp_node_running(tcp_node_t tcp_node){
	return tcp_node->running;
}
// returns whether ip_node running still
int tcp_node_ip_running(tcp_node_t tcp_node){
	return ip_node_running(tcp_node->ip_node);
}
/**********************************************************************/

static void _handle_read(tcp_node_t tcp_node){
	// dequeue next packets to read  -- iterate through to make sure to handle each packet on queue
	bqueue_t *to_read;
	to_read = tcp_node->to_read;
	
	tcp_packet_data_t *next_packet = (tcp_packet_data_t *)malloc(sizeof(tcp_packet_data_t));
	
	while(!(bqueue_empty(to_read))){
		// dequeue next packet in to_read queue
		bqueue_dequeue(to_read, (void **)&next_packet);
		// for now just print packet:
		printf("Packet received: %s\n", next_packet->packet);
	}	
	
	free(next_packet);
}







