/* tcp_connection.c file
	struct for handling each tcp_connection.  
		Has socket_id that the kernal tcp_node can store to refer to.
		maintains own statemachine
		maintains own sliding window protocol
		owns:
			local tcp_socket_address{virt_ip, virt_port}
			remote tcp_socket_address
			
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <inttypes.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#include "tcp_connection.h"
#include "tcp_utils.h"
#include "tcp_connection_state_machine_handle.h"

#define SYN_TIMEOUT 2 //2 seconds at first, and doubles each time next syn_sent
#define CRASH_AND_BURN 4567829


// all those fancy things we defined here are now located in tcp_utils so they can also 
// be shared with tcp_connection_state_handle

struct tcp_connection{
	int socket_id;	// also serves as index of tcp_connection in tcp_node's tcp_connections array
	
	/* Needs mutex and signaling mechanism to interact with tcp_api as well as return value for tcp_api to read off*/
	pthread_mutex_t api_mutex;
	pthread_cond_t api_cond;
	int api_ret;
	
	tcp_socket_address_t local_addr;
	tcp_socket_address_t remote_addr;
	// owns state machine
	state_machine_t state_machine;

	// owns window for sending and window for receiving
	send_window_t send_window;
	recv_window_t receive_window;
	
	// owns accept queue to queue new connections when listening and receive syn
	// always queues -- if user called accept then we'll call out tcp_connection_api_finish
	// which will be a call to finish v_accept with tcp_api_accept_finish <-- the blocking call
	// and dequeue that new established connection
	bqueue_t *accept_queue;  
	
	/* these really only need to be set when we send
	   off the first seq, just to make sure that the 
	   ack that we get back is valid */
	uint32_t last_seq_received;
	uint32_t last_seq_sent;	
	
	// keeping track of if need to fail connect attempt or retry
	int syn_count;
	struct timeval connect_accept_timer;
	
	// needs reference to the to_send queue in order to queue its packets
	bqueue_t *to_send;	//--- tcp data for ip to send
	// when tcp_node demultiplexes packets, gives packet to tcp_connection by placing packet on its my_to_read queue
	bqueue_t *my_to_read; // holds tcp_packet_data_t's 

	pthread_t read_send_thread; //has thread that handles the my_to_read queue and window timeouts in a loop

	int running; //are we running still?  1 for true, 0 for false -- indicates to thread to shut down
};

/* TODO: Start using this in our implemenation:
give to tcp_connection:

	tcp_connection	
		int ret_value; // return value for the calling tcp_api function
		pthread_mutex_t api_mutex
		pthread_cond_t api_cond
		// now when a tcp_api function calls, it will lock the mutex, and wait on the api_cond for the 
		//tcp connection to finish its duties
*/
pthread_mutex_t tcp_connection_get_api_mutex(tcp_connection_t connection){
	return connection->api_mutex;
}

pthread_cond_t tcp_connection_get_api_cond(tcp_connection_t connection){
	return connection->api_cond;
}

int tcp_connection_get_api_ret(tcp_connection_t connection){
	return connection->api_ret;
}
		
/* calls pthread_cond_signal(api_cond) so that the waiting tcp_api function can stop waiting and take a look at the return value		
tcp_connection_api_signal(connection); 
*/
void tcp_connection_api_signal(tcp_connection_t connection, int ret){
	/* set return value and signal that tcp_api function finished on the connection's part */
	connection->api_ret = ret;
	
	pthread_cond_t api_cond = connection->api_cond;
	pthread_cond_signal(&api_cond);
}

tcp_connection_t tcp_connection_init(int socket, bqueue_t *tosend){
	// init state machine
	state_machine_t state_machine = state_machine_init();

	tcp_connection_t connection = (tcp_connection_t)malloc(sizeof(struct tcp_connection));
	
	/* Set what it needs in order to interact with tcp_api */
	pthread_mutex_init(&(connection->api_mutex), NULL);
	pthread_cond_init(&(connection->api_cond), NULL);
	
	connection->api_ret = CRASH_AND_BURN;
	
	// let's do this the first time we send the SYN, just so if we try to send before that
	// we'll crash and burn because it's null

	connection->send_window = NULL;
	connection->receive_window = NULL;

	connection->socket_id = socket;
	//zero out virtual addresses to start
	connection->local_addr.virt_ip = 0;
	connection->local_addr.virt_port = 0;
	connection->remote_addr.virt_ip = 0;
	connection->remote_addr.virt_port = 0;
	
	connection->state_machine = state_machine;
	connection->accept_queue = NULL;  //initialized when connection goes to LISTEN state
	connection->to_send = tosend;
	
	/* 	I know that this will set it to a huge number and not
		actually -1, I learned my lesson. These variables
		are used to hold on to the seq/acks we've received
		before we hand the logic off to the state machine */	
	connection->last_seq_received = -1;
	connection->last_seq_sent 	  = -1;

	state_machine_set_argument(state_machine, connection);		
	
	// init my_to_read queue
	bqueue_t *my_to_read = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(my_to_read);
	connection->my_to_read = my_to_read;
	
	connection->running = 1;
	
	//start read_thread
	/* Initialize and set thread detached attribute */
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		
	int status = pthread_create(&(connection->read_send_thread), &attr, _handle_read_send, (void *)connection);
    if (status){
         printf("ERROR; return code from pthread_create() for _handle_read_ack is %d\n", status);
         return 0;
    }
    pthread_attr_destroy(&attr);
	
	return connection;
}

void tcp_connection_destroy(tcp_connection_t connection){
	
	connection->running = 0;
	
	/* Destroy mutex and signal */
	pthread_mutex_destroy(&(connection->api_mutex));
	pthread_cond_destroy(&(connection->api_cond));
	
	// destroy windows
	if(connection->send_window)
		send_window_destroy(&(connection->send_window));
	if(connection->receive_window)
		recv_window_destroy(&(connection->receive_window));
	
	// destroy state machine
	state_machine_destroy(&(connection->state_machine));
	tcp_connection_accept_queue_destroy(connection);
	
	// cancel read_thread
	int rc = pthread_join(connection->read_send_thread, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_cancel() for tcp_connection of socket %d is %d\n", connection->socket_id, rc);
		exit(-1);
	}
	
	// take all packets off my_to_read queue and destroys queue
	tcp_packet_data_t tcp_packet_data;
	while(!bqueue_trydequeue(connection->my_to_read, (void**)&tcp_packet_data))
		tcp_packet_data_destroy(tcp_packet_data);	
	
	bqueue_destroy(connection->my_to_read);
						
	free(connection);
	connection = NULL;
}

/* ////////////////////////////////////////////// */
/**************** Receiving Packets ***************/

/* 
validates an ACK when it's being received during 
establishing the connection 
*/
int _validate_ack(tcp_connection_t connection, uint32_t ack){
	/* the ACK is valid if it is equal to one more than the 
		last sequence number sent */
	if (ack != (connection->last_seq_sent + 1) % MAX_SEQNUM)
		return -1;

	return 0;
}
/* Function for tcp_node to call to place a packet on this connection's
	my_to_read queue for this connection to handle in its _handle_read_send thread 
	returns 1 on success, 0 on failure */
int tcp_connection_queue_to_read(tcp_connection_t connection, tcp_packet_data_t tcp_packet){
	
	if(bqueue_enqueue(connection->my_to_read, tcp_packet))
		return 0;

	return 1;
}
/* Called when connection in LISTEN state receives a syn.  
	Queues info necessary to create a new connection when accept called 
	returns 0 on success, negative if failed -- ie queue destroyed */
int tcp_connection_handle_syn(tcp_connection_t connection, 
		uint32_t local_ip,uint32_t remote_ip, uint16_t remote_port, uint32_t seqnum){ 
	
	/* create accept_queue_data to load up with necessary info and queue for accept call */
	accept_queue_data_t data = accept_queue_data_init(local_ip, remote_ip, remote_port, seqnum);
	return bqueue_enqueue(connection->accept_queue, data);
}											

/*
tcp_connection_handle_receive_packet
	as needed. All meta-information (SYN, FIN, etc.) will probably be 
	passed in to the state-machine, which will take appropriate action
	with state changes. Therefore, the logic of this function should 
	mostly be concerned with passing off the data to the window/validating
	the correctness of the received packet (that it makes sense) 
*/

void tcp_connection_handle_receive_packet(tcp_connection_t connection, tcp_packet_data_t tcp_packet_data){
	puts("tcp_connection_receive_packet: received packet");
	/* RFC 793: 
		Although these examples do not show connection synchronization using data
		-carrying segments, this is perfectly legitimate, so long as the receiving TCP
  		doesn't deliver the data to the user until it is clear the data is valid
		

		so no matter what, we need to be pushing the data to the receiving window, 
		and we simply shouldn't call get_next until we're in the established state */

	void* tcp_packet = tcp_packet_data->packet;
	
	//TODO: FIGURE OUT WHEN ITS NOT APPROPRIATE TO RESET REMOTE ADDRESSES -- we don't want our connection sabotaged 
	//reset remote ip/port in case it has changed + so that we can correctly calculate checksum	
	if(tcp_connection_get_state(connection) == LISTEN){
		/* since listen binds to all interfaces, must be able to reset its ip addresses to receive connect requests */
		tcp_connection_set_remote(connection, tcp_packet_data->remote_virt_ip, tcp_source_port(tcp_packet));
		tcp_connection_set_local_ip(connection, tcp_packet_data->local_virt_ip);
	}
	
	/* ensure the integrity */
	int checksum_result = tcp_utils_validate_checksum(tcp_packet, 
											tcp_packet_data->packet_size, 
											connection->remote_addr.virt_ip, // this is the local IP of the sender, 
											connection->local_addr.virt_ip,  // so the pseudo header will match this order
											TCP_DATA);
	if(checksum_result < 0){
		puts("Bad checksum! what happened? not discarding");
		return;
	}
	
	//TODO: ONLY PUSH DATA TO RECEIVE WINDOW WHEN OK TO DO SO
	
	/* check if there's any data, and if there is push it to the window,
		but what does the seqnum even mean if the ACKs haven't been synchronized? */
	memchunk_t data = tcp_unwrap_data(tcp_packet, tcp_packet_data->packet_size);
	if(data){ 
		printf("tcp_connection_receive_packet: received packet with data: \n %s\n", (char*)data->data);
		uint32_t seqnum = tcp_seqnum(tcp_packet);	
		recv_window_receive(connection->receive_window, data->data, data->length, seqnum);
	}	
	/* now check the bits */
	if(tcp_syn_bit(tcp_packet) && tcp_ack_bit(tcp_packet)){
		//puts("received packet with syn_bit and ack_bit set");
		if(_validate_ack(connection, tcp_ack(tcp_packet)) < 0){
			/* then you sent a syn with a seqnum that wasn't faithfully returned. 
				what should we do? for now, let's discard */
			puts("Received invalid ack with SYN/ACK. Discarding.");
			return;
		}

		
		/* received a SYN/ACK, record the seq you got, and validate
			that the ACK you received is correct */
		connection->last_seq_received = tcp_seqnum(tcp_packet);
		
		// this function calls tcp_connection_api_finish if in SYN_SENT
		state_machine_transition(connection->state_machine, receiveSYN_ACK); 
	}

	else if(tcp_syn_bit(tcp_packet)){
		//puts("received packet with syn_bit set");
		/* got a SYN -- only valid changes are LISTEN_to_SYN_RECEIVED or SYN_SENT_to_SYN_RECEIVED */ 
		
		if(state_machine_get_state(connection->state_machine) == SYN_SENT){		
		
			tcp_connection_set_remote(connection, tcp_packet_data->remote_virt_ip, tcp_source_port(tcp_packet));
			
			/* set the last_seq_received, then pass off to state machine to make transition SYN_SENT_to_SYN_RECEIVED */	
			connection->last_seq_received = tcp_seqnum(tcp_packet);
			state_machine_transition(connection->state_machine, receiveSYN);
		}
		else if(state_machine_get_state(connection->state_machine) == LISTEN){
			/* create a new connection will be initiated with below triple and LISTEN state and then
				go through the LISTEN_to_SYN_RECEIVED transition.  -- will queue that connection
				and v_accept will call to dequeue it */
		
			// listening connection just reset its remote/local ip with where this packet came from
			// so this is what the new connection should be initialized with
			tcp_connection_handle_syn(connection, tcp_connection_get_local_ip(connection),
										tcp_connection_get_remote_ip(connection), 
										tcp_source_port(tcp_packet), 
										tcp_seqnum(tcp_packet));	
			// anything else??		
		}
	}
	/* this will almost universally be true. */
	else if(tcp_ack_bit(tcp_packet)){
		if(_validate_ack(connection, tcp_ack(tcp_packet)) < 0){
			// should we drop the packet here?
			puts("Received invalid ack with SYN/ACK. Discarding.");
			return;
		}
		
		//reset remote ip/port in case it has changed
		tcp_connection_set_remote(connection, tcp_packet_data->remote_virt_ip, tcp_source_port(tcp_packet));
		
		//TODO: ***************todo *********************
		
		/* careful, this might be NULL -- shouldn't we actually do this in the state_machine transition?*/
		//send_window_ack(connection->send_window, tcp_ack(tcp_packet));
		
		// should we give this ack to the receive window?
		
		state_machine_transition(connection->state_machine, receiveACK);
	}
			
	/* Umm, anything else ? */	
	// FREE IT??????
}

/* 0o0o0oo0o0o0o0o0o0o0o SENDING o0o0o0ooo0o0o0o0o0o0o0o0o0oo0o */


/* 
tcp_connection_queue_ip_send
	this is the reason that tcp_wrap_packet_send needs to be defined 
	in tcp_connection.c, because otherwise tcp_utils would need to 
	#include or somehow know about the queue which tcp_connection
	is using. 
*/
int tcp_connection_queue_ip_send(tcp_connection_t connection, tcp_packet_data_t packet){
	/* if the queue isn't there, then you're probably testing, so just
		print it out for debugging purposes */
	if(!connection->to_send){
		printf("Trying to print packet: ");
		tcp_packet_print(packet);
		return 1;
	}

	return bqueue_enqueue(connection->to_send, packet);
}
	

/*
   NOTE should probably be here just because it's really would be a method 
   if we had classes, it takes in and relies upon the tcp_connection_t implementation 
	 
   I also left the original version intact in src/tcp/tcp_utils.s
*/
int tcp_wrap_packet_send(tcp_connection_t connection, struct tcphdr* header, void* data, int data_len){	
	
	uint32_t total_length = tcp_offset_in_bytes(header) + data_len;
	
	// if we're sending data, concatenate header and data into one packet
	if((data != NULL)&&(data_len)){
		/* data_len had better be the same size as when you called tcp_header_init()!! */
		memcpy(header+tcp_offset_in_bytes(header), data, data_len);
		free(data);
	}
	
	
	// tcp checksum calculated on BOTH header and data
	tcp_utils_add_checksum(header, total_length, connection->local_addr.virt_ip, connection->remote_addr.virt_ip, TCP_DATA);//TCP_DATA is tcp protocol number right?
	
	// send off to ip_node as a tcp_packet_data_t
	//tcp_packet_data_t packet_data = tcp_packet_data_init(packet, data_len+TCP_HEADER_MIN_SIZE, local_virt_ip, remote_virt_ip);
	tcp_packet_data_t packet_data = tcp_packet_data_init(
										(char*)header, 
										total_length,
										tcp_connection_get_local_ip(connection),
										tcp_connection_get_remote_ip(connection));
										
	// no longer need packet
	free(header);
	
	if(tcp_connection_queue_ip_send(connection, packet_data) < 0){
		//TODO: HANDLE!
		puts("Something wrong with sending tcp_packet to_send queue--How do we want to handle this??");	
		free(packet_data);
		return -1;
	}

	return 1;
}

void tcp_connection_send_next_chunk(tcp_connection_t connection, send_window_chunk_t next_chunk){
	struct tcphdr* header = tcp_header_init(connection->local_addr.virt_port, connection->remote_addr.virt_port, next_chunk->length);
	
	/* set the ack bit, and get the ack to send from 
		the window. ACK's should ALWAYS be sent (if established) */
	tcp_set_ack_bit(header);
	tcp_set_ack(header, recv_window_get_ack(connection->receive_window));
	
	/* the seqnum should be the seqnum in the next_chunk */
	tcp_set_seq(header, next_chunk->seqnum);
	
	/* send it off!  */
	tcp_wrap_packet_send(connection, header, next_chunk->data, next_chunk->length);
}

/* 
tcp_connection_push_data
	just pushes the given data into the sending window. Likely followed by a get_next loop 
*/
void tcp_connection_push_data(tcp_connection_t connection, void* data, int data_len){
	/* if the window doesn't exist, you can't write because you haven't 
		passed through the right states */
	if(connection->send_window == NULL) 
		return;

	send_window_push(connection->send_window, data, data_len);
}


// queues chunks off from send_window and handles sending them for as long as send_window wants to send more chunks
// NOTE: this presents the danger of one greedy connection that blocks all the other ones from sending
int tcp_connection_send_next(tcp_connection_t connection){

	int bytes_sent = 0;
	send_window_chunk_t next_chunk;
	send_window_t send_window = connection->send_window;

	// keep sending as many chunks as window has available to give us
	while((next_chunk = send_window_get_next(send_window))){
	
		// send it off
		tcp_connection_send_next_chunk(connection, next_chunk);

		// increment bytes_sent
		bytes_sent += next_chunk->length;
	}	

	return bytes_sent;
}

int tcp_connection_send_data(tcp_connection_t connection, const unsigned char* to_write, int num_bytes){
	// push data to window and then send as much as we can
	tcp_connection_push_data(connection, (void*)to_write, num_bytes);	

	// send as much data right now as send_window allows
	int ret = tcp_connection_send_next(connection);
	return ret;
}


/********************* End of Sending Packets ********************/

uint16_t tcp_connection_get_local_port(tcp_connection_t connection){
	/*  given the frequency of this call (whenever we send a chunk), 
		we should make it as efficient as possible, which includes
		pushing as little onto the stack as necessary */
	return connection->local_addr.virt_port;
/*	tcp_socket_address_t addr;
	addr = connection->local_addr;	
	uint16_t virt_port = addr.virt_port;
	return virt_port;
*/
}
uint16_t tcp_connection_get_remote_port(tcp_connection_t connection){
	/* see note above */
	return connection->remote_addr.virt_port;
/*	
	tcp_socket_address_t addr = connection->remote_addr;	
	uint16_t virt_port = addr.virt_port;
	return virt_port;
*/
}

void tcp_connection_set_local_port(tcp_connection_t connection, uint16_t port){
	connection->local_addr.virt_port = port;
}

uint32_t tcp_connection_get_local_ip(tcp_connection_t connection){
	tcp_socket_address_t addr = connection->local_addr;
	uint32_t virt_ip = addr.virt_ip;
	return virt_ip;
}
uint32_t tcp_connection_get_remote_ip(tcp_connection_t connection){
	tcp_socket_address_t addr = connection->remote_addr;
	uint32_t virt_ip = addr.virt_ip;
	return virt_ip;
}
int tcp_connection_get_socket(tcp_connection_t connection){
	return connection->socket_id;
}
/******************* Window getting and setting and destroying functions ****************************/
/******************* Window getting and setting and destroying functions ****************************/
	
/************** Sending Window *********************/
// what's the point of these helper functions?
/*
 send_window_t tcp_connection_send_window_init(tcp_connection_t connection, double timeout, int send_window_size, int send_size, int ISN){
	connection->send_window = send_window_init(timeout, send_window_size, send_size, ISN);
	return connection->send_window;
}

send_window_t tcp_connection_get_send_window(tcp_connection_t connection){
	return connection->send_window;
}

// we should destroy the window when we close connections
void tcp_connection_send_window_destroy(tcp_connection_t connection){	
	if(connection->send_window)
		send_window_destroy(&(connection->send_window));	
	connection->send_window = NULL;
}
*/
/************** End of Sending Window *********************/

/************** Receiving Window *********************/
/*
recv_window_t tcp_connection_recv_window_init(tcp_connection_t connection, uint32_t window_size, uint32_t ISN){
	connection->receive_window = recv_window_init(window_size, ISN);
	return connection->receive_window;
}
recv_window_t tcp_connection_get_recv_window(tcp_connection_t connection){
	return connection->receive_window;
}
// we should destroy the window when we close connections
void tcp_connection_recv_window_destroy(tcp_connection_t connection){	
	if(connection->receive_window)
		recv_window_destroy(&(connection->receive_window));	
	connection->receive_window = NULL;
}
*/
/************** End of Receiving Window *********************/

/******************* End of Window getting and setting functions *****************************************/
/******************* End of Window getting and setting functions *****************************************/


/*0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0 to_read Thread o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0*/

// runs thread for tcp_connection to handle sending/reading and keeping track of its packets/acks
void *_handle_read_send(void *tcpconnection){
	
	tcp_connection_t connection = (tcp_connection_t)tcpconnection;

	struct timespec wait_cond;	
	struct timeval now;	// keep track of time to compare to window timeouts and connections' connect_accept_timer
	void* packet;
	int ret;

	while(connection->running){	
		gettimeofday(&now, NULL);	
		wait_cond.tv_sec = now.tv_sec+PTHREAD_COND_TIMEOUT_SEC;
		wait_cond.tv_nsec = 1000*now.tv_usec+PTHREAD_COND_TIMEOUT_NSEC;
		
		ret = bqueue_timed_dequeue_abs(connection->my_to_read, &packet, &wait_cond);
		if (ret != 0) 
			/* should probably check at this point WHY we failed (for instance perhaps the queue
				was destroyed */
			continue;
		
		//handle to read packet
		tcp_connection_handle_receive_packet(connection, packet);
		
		if(tcp_connection_get_state(connection)==SYN_SENT){	
			if(difftime(now, connection->connect_accept_timer) > (1 << ((connection->syn_count)-1))*SYN_TIMEOUT){
				// we timeout connect or resend
				if((connection->syn_count)>2){
					// timeout connection attempt
					connection->syn_count = 0;
					tcp_connection_state_machine_transition(connection, CLOSE);
				}
				else{	
					// resend syn
					tcp_connection_send_syn(connection);
					connection->syn_count = connection->syn_count+1;
				}
			}
		}/*
		else if(tcp_connection_get_state(connection)==ESTABLISHED){
			check_timers(my to_send);
			chunk_t chunk;
			while(chunk = get_next(my to_send)){
				send(chunk);
			}
		}*/
	}
	pthread_exit(NULL);
}

/************* Functions regarding the accept queue ************************/
	/* The accept queue is initialized when the server goes into the listen state.  
		Destroyed when leaves LISTEN state 
		Each time a syn is received, a new tcp_connection is created in the SYN_RECEIVED state and queued */


void tcp_connection_accept_queue_destroy(tcp_connection_t connection){
	bqueue_t *q = connection->accept_queue;
	if(q==NULL)
		return;
		
	// need to destroy each connection on the accept queue before destroying queue
	tcp_accept_data_t data = NULL;
	while(!bqueue_trydequeue(q, (void**)&data)){
		tcp_accept_data_destroy(&data);
	}
	bqueue_destroy(q);
	connection->accept_queue = NULL;
}


// return popped triple -- null if error in dequeue
accept_queue_data_t tcp_connection_accept_queue_dequeue(tcp_connection_t connection){
	bqueue_t *q = connection->accept_queue;
	if(q==NULL){
		puts("Error in tcp_connection_accept_queue_dequeue: SEE CODE");
		return NULL;
	}
	accept_queue_data_t data;
	int ret = bqueue_dequeue(q, (void**)&data);
	if(ret<0)
		return NULL;
	return data;
}

/************* End of Functions regarding the accept queue ************************/


void tcp_connection_set_last_seq_received(tcp_connection_t connection, uint32_t seq){
	connection->last_seq_received = seq;
}

uint32_t tcp_connection_get_last_seq_received(tcp_connection_t connection){
	return connection->last_seq_received;
}
uint32_t tcp_connection_get_last_seq_sent(tcp_connection_t connection){
	return connection->last_seq_sent;
}



int tcp_connection_state_machine_transition(tcp_connection_t connection, transition_e transition){
	tcp_connection_print_state(connection);
	int ret = state_machine_transition(connection->state_machine, transition);
	tcp_connection_print_state(connection);
	return ret;
}

/*
state_machine_t tcp_connection_get_state_machine(tcp_connection_t connection){
	return connection->state_machine;
}
*/

state_e tcp_connection_get_state(tcp_connection_t connection){
	return state_machine_get_state(connection->state_machine);
}

void tcp_connection_set_state(tcp_connection_t connection, state_e state){
	state_machine_set_state(connection->state_machine, state);
}

void tcp_connection_print_state(tcp_connection_t connection){
	state_machine_print_state(connection->state_machine);	
}


// to print when user calls 'sockets'
void tcp_connection_print_sockets(tcp_connection_t connection){

	int socket, local_port, local_ip, remote_port, remote_ip;
	char local_buffer[INET_ADDRSTRLEN], remote_buffer[INET_ADDRSTRLEN];
	struct in_addr local, remote;

	socket = tcp_connection_get_socket(connection);
	local_port = tcp_connection_get_local_port(connection);
	local_ip = tcp_connection_get_local_ip(connection);
	remote_port = tcp_connection_get_remote_port(connection);
	remote_ip = tcp_connection_get_remote_ip(connection);
		
	local.s_addr = local_ip; 
	remote.s_addr = remote_ip;
	
	inet_ntop(AF_INET, &local, local_buffer, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &remote, remote_buffer, INET_ADDRSTRLEN);

	printf("\n[Socket %d]:\n", socket);
	printf("\t<Local Port: %d, Local IP: (%u) %s > <Remote Port: %d, Remote IP: (%u) %s > <State: ", 
				local_port, local_ip, local_buffer, remote_port, remote_ip, remote_buffer);
	tcp_connection_print_state(connection);
	printf(">\n");

}

/* FOR TESTING */
void tcp_connection_print(tcp_connection_t connection){
	puts( "IT WORKS!!" );
}
	
void tcp_connection_set_remote(tcp_connection_t connection, uint32_t remote, uint16_t port){
	connection->remote_addr.virt_ip = remote;
	connection->remote_addr.virt_port = port;
}
void tcp_connection_set_remote_ip(tcp_connection_t connection, uint32_t remote_ip){
	connection->remote_addr.virt_ip = remote_ip;
}
void tcp_connection_set_local_ip(tcp_connection_t connection, uint32_t ip){
	connection->local_addr.virt_ip = ip;
}

/* hacky? */  //<-- yeah kinda
#include "tcp_connection_state_machine_handle.c"

	
