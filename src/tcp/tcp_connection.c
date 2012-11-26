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
//#include <netinet/tcp.h> -- we define it in utils.h!
#include <inttypes.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#include "tcp_node.h" // in tcp_node.h #include "tcp_connection.h"
#include "tcp_utils.h"
#include "tcp_connection_state_machine_handle.h"


// all those fancy things we defined here are now located in tcp_utils so they can also 
// be shared with tcp_connection_state_handle

struct tcp_connection{
	
	tcp_node_t tcp_node; // needs reference to node in order to properly take itself out of kernal on timeouts etc
	
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
	int recv_window_alive; // we use this for implementing shutdown read option -- instead of destroying our
							// existing receive window we just keep track of this boolean - 1 for alive, 0 for down
	
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
	uint32_t fin_seqnum; //if we send fin, set our fin_seqnum to this, or if we receive fin, for acking purposes
	
	// keeping track of if need to fail connect attempt or retry
	int syn_fin_count;
	struct timeval state_timer; //WAS syn_timer, but is now an all purpose timer
		//for example, don't want to block on accept forever, so we use this to time out of waiting for
		//SYN_RECEIVED to change to ESTABLISHED
		// lets also use this for closing
	
	// needs reference to the to_send queue in order to queue its packets
	bqueue_t *to_send;	//--- tcp data for ip to send
	// when tcp_node demultiplexes packets, gives packet to tcp_connection by placing packet on its my_to_read queue
	bqueue_t *my_to_read; // holds tcp_packet_data_t's 

	pthread_t read_send_thread; //has thread that handles the my_to_read queue and window timeouts in a loop

	int closing; //have we requested to close yet? 0 when either in CLOSED state of CLOSE requested, 1 otherwise
	int running; //are we running still?  1 for true, 0 for false -- indicates to thread to shut down
};

/* TODO: Start using this in our implemenation:
give to tcp_connection:

	tcp_connection	
		int api_ret; // return value for the calling tcp_api function
		pthread_mutex_t api_mutex
		pthread_cond_t api_cond
		// now when a tcp_api function calls, it will lock the mutex, and wait on the api_cond for the 
		//tcp connection to finish its duties
*/
pthread_mutex_t* tcp_connection_get_api_mutex(tcp_connection_t connection){
	return &(connection->api_mutex);
}

pthread_cond_t tcp_connection_get_api_cond(tcp_connection_t connection){
	return connection->api_cond;
}

int tcp_connection_get_api_ret(tcp_connection_t connection){
	return connection->api_ret;
}
	
tcp_connection_t tcp_connection_init(tcp_node_t tcp_node, int socket, bqueue_t *tosend){
	tcp_connection_t connection = (tcp_connection_t)malloc(sizeof(struct tcp_connection));
	
	connection->tcp_node = tcp_node;
	connection->closing = 1; //start off in CLOSED state when initialized
	connection->running = 1;
	
	/* Set what it needs in order to interact with tcp_api */
	pthread_mutex_init(&(connection->api_mutex), NULL);
	pthread_cond_init(&(connection->api_cond), NULL);
	
	connection->api_ret = SIGNAL_CRASH_AND_BURN;
	
	/* we init send window here but only init recv window when we get our first seqnum */
	uint32_t ISN = RAND_ISN();	
	connection->send_window = send_window_init(WINDOW_DEFAULT_TIMEOUT, DEFAULT_WINDOW_SIZE, DEFAULT_WINDOW_CHUNK_SIZE, ISN);

	connection->receive_window = NULL;
	connection->recv_window_alive = 1; //I set it to one for the purpose of knowing how to set window size in tcp_wrap_packet_send

	connection->socket_id = socket;
	//zero out virtual addresses to start
	connection->local_addr.virt_ip = 0;
	connection->local_addr.virt_port = 0;
	connection->remote_addr.virt_ip = 0;
	connection->remote_addr.virt_port = 0;
	
	connection->state_machine = state_machine_init();
	state_machine_set_argument(connection->state_machine, connection);		

	connection->accept_queue = NULL;  //initialized when connection goes to LISTEN state
	connection->to_send = tosend;

	/* 	I know that this will set it to a huge number and not
		actually -1, I learned my lesson.  (LOL -alex) These variables
		are used to hold on to the seq/acks we've received
		before we hand the logic off to the state machine */	
	connection->last_seq_received = -1;
	connection->last_seq_sent 	  = -1;
	
	// init my_to_read queue
	bqueue_t *my_to_read = (bqueue_t*) malloc(sizeof(bqueue_t));
	bqueue_init(my_to_read);
	connection->my_to_read = my_to_read;
	
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

	// >> do this immediately! because it depends on the things you're destroying! <<
	// cancel read_thread
	int rc = pthread_join(connection->read_send_thread, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_cancel() for tcp_connection of socket %d is %d\n", connection->socket_id, rc);
		exit(-1);
	}
	// tell everyone who is waiting on this thread that
	// the connection is being destroyed
	tcp_connection_api_signal(connection, SIGNAL_DESTROYING);

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
	
	// take all packets off my_to_read queue and destroys queue
	tcp_packet_data_t tcp_packet_data;
	while(!bqueue_trydequeue(connection->my_to_read, (void**)&tcp_packet_data))
		tcp_packet_data_destroy(&tcp_packet_data);	
	
	bqueue_destroy(connection->my_to_read);
	free(connection->my_to_read);
						
	free(connection);
	connection = NULL;
}

/* ////////////////////////////////////////////// */
/**************** Receiving Packets ***************/

/* 
validates an ACK when it's being received during 
establishing the connection 
Returns 0 on success, -1 on failure
*/
int _validate_ack(tcp_connection_t connection, uint32_t ack){
	if(tcp_connection_get_state(connection) == SYN_SENT){
		if((connection->last_seq_sent + 1) == ack)
			return 0;
		else
			return -1;
	}
	else if(tcp_connection_in_closing_state(connection)){
		// could be the ack for our FIN
		if(ack == (connection->fin_seqnum)+1)
			return 0;
	}
	else if(!connection->send_window)
		return -1; //then we probably shouldn't be receiving an ack, right?
		
	return send_window_validate_ack(connection->send_window, ack);
}	

/* Function for tcp_node to call to place a packet on this connection's
	my_to_read queue for this connection to handle in its _handle_read_send thread 
	returns 1 on success, 0 on failure */
int tcp_connection_queue_to_read(tcp_connection_t connection, tcp_packet_data_t tcp_packet){
	print(("queueing packet"), TCP_PRINT);
	if(bqueue_enqueue(connection->my_to_read, tcp_packet))
		return 0;
	else
		return 1;
}

/* Called when connection in LISTEN state receives a syn.  
	Queues info necessary to create a new connection when accept called 
	returns 0 on success, negative if failed -- ie queue destroyed */
int tcp_connection_handle_syn_LISTEN(tcp_connection_t connection, 
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

	void* tcp_packet = tcp_packet_data->packet;
	
	/* Printing packet here */
	if(state_machine_get_state(connection->state_machine) != ESTABLISHED){
		print(("Received Packet of size %d", tcp_packet_data->packet_size), PACKET_PRINT);
		view_packet((struct tcphdr*)tcp_packet, tcp_packet+20, tcp_packet_data->packet_size-20); 
	}
	
	/* First thing: ensure the integrity */
	int checksum_result = tcp_utils_validate_checksum(tcp_packet, 
											tcp_packet_data->packet_size, 
											tcp_packet_data->remote_virt_ip, // because we might not know
											tcp_packet_data->local_virt_ip,// our address right now, 
																			// and this is still valid?
											TCP_DATA);
	/* CHECKSUM */
	if(checksum_result < 0){
		puts("Bad checksum! what happened? not discarding");
		//tcp_packet_data_destroy(&tcp_packet_data);
		//return;
	}	
	/* this boolean just decides whether we need
		to update our peer (just call tcp_wrap_packet_send) with
		our current state, at this point this only involves sending
		an ack back which we will always do whenever we send a packet,
		but if we receive data and then don't need to send a packet, 
		this boolean will make sure we send one just for that reason */
	int update_peer = 0;	
 	
 	state_e state = state_machine_get_state(connection->state_machine);	
		
//SEE PAGE 64 OF RFC 793: IN THE FOLLOWING SECTION I FOLLOW IT PRECISELY

	/* DESIGN CHOICE:  We check the ack field before the fin field so that we can handle 
		the transition FIN_WAIT_1 to TIME_WAIT (ie receiving an ack+fin) in this entire segment.
		This means that in this segment of code, if a ack+fin received,
		 the connection can move from the FIN_WAIT_1 state to the FIN_WAIT_2 state (when we check the ack)
		 and then from the FIN_WAIT_2 state to the TIME_WAIT state when we then check the fin 
		 This means we don't need a state machine transition FIN_WAIT_1_to_TIME_WAIT */
	
	// if closed we're supposedly fictional
	if(state == CLOSED){
		tcp_connection_refuse_connection(connection, tcp_packet_data); //sends rst if rst not set in packet
		tcp_packet_data_destroy(&tcp_packet_data);
		return;
	}
	else if(state == LISTEN){
		/* RFC If the state is LISTEN then:
			first check for an RST.  An incoming RST should be ignored.  Return. */
		if(tcp_rst_bit(tcp_packet)){
			tcp_packet_data_destroy(&tcp_packet_data);
			return;
		}
		/* second check for an ACK*/
		else if(tcp_ack_bit(tcp_packet)){
        	/* Any acknowledgment is bad if it arrives on a connection still in the LISTEN state.  
        	An acceptable reset segment should be formed. Return. */
        	tcp_connection_refuse_connection(connection, tcp_packet_data);
        }
		/* third check for a SYN */
		else if(tcp_syn_bit(tcp_packet)){
			/* handle like we were before I redid this I suppose */

			/* since listen binds to all interfaces, must be able to reset its ip addresses to receive connect requests */
			tcp_connection_set_remote(connection, tcp_packet_data->remote_virt_ip, tcp_source_port(tcp_packet));
			tcp_connection_set_local_ip(connection, tcp_packet_data->local_virt_ip);
		
            tcp_connection_handle_syn(connection, tcp_packet_data);
        }
        tcp_packet_data_destroy(&tcp_packet_data);
        return;
    }
    /* If the state is SYN-SENT then */
    else if(state == SYN_SENT){
    	/* first check the ACK bit */
    	if(tcp_ack_bit(tcp_packet)){
    		/* If SEG.ACK =< ISS, or SEG.ACK > SND.NXT, send a reset 
    		(unless the RST bit is set, if so drop the segment and return) */
			if((tcp_ack(tcp_packet)<=connection->last_seq_sent)||(tcp_ack(tcp_packet)>connection->last_seq_sent+1)){
				printf("[Socket %d]: Received invalid ack while in the SYN_SENT state -- dropping packet\n", 
					connection->socket_id);
				if(tcp_rst_bit(tcp_packet)){
    				tcp_packet_data_destroy(&tcp_packet_data);
					return; //instead of sending reset just return
				}
				else{
					tcp_connection_refuse_connection(connection, tcp_packet_data); //send reset
				}
			}
			else{	/* If SND.UNA =< SEG.ACK =< SND.NXT then the ACK is acceptable. */
          		        
          		/* If the RST bit is set */
          		if(tcp_rst_bit(tcp_packet)){
					/*If the ACK was acceptable then signal the user "error: connection reset", 
					drop the segment, enter CLOSED state, delete TCB, and return.  
					Otherwise (no ACK) drop the segment and return. */
					state_machine_transition(connection->state_machine, receiveRST);
				}
				else{
    				if(tcp_syn_bit(tcp_packet)){
    					/* syn-ack */
    					tcp_connection_handle_syn_ack(connection, tcp_packet_data);
    				}
    			}
    		}
    	}
    	else if(tcp_syn_bit(tcp_packet)){
             // we think the TA implementation is WRONG and now we have to deal with it WTF
            //tcp_connection_handle_syn_ack(connection, tcp_packet_data);
            tcp_connection_handle_syn(connection, tcp_packet_data);   		
    	}
    	tcp_packet_data_destroy(&tcp_packet_data);
    	return;
    }	
    else{		
	    /*Otherwise first check sequence number */
			/* SYN-RECEIVED STATE, ESTABLISHED STATE, FIN-WAIT-1 STATE, FIN-WAIT-2 STATE,
      			CLOSE-WAIT STATE, CLOSING STATE, LAST-ACK STATE, TIME-WAIT STATE */
		
		/*If the RCV.WND is zero, no segments will be acceptable, but 
        special allowance should be made to accept valid ACKs, URGs and
        RSTs. */

		if(connection->receive_window != NULL){
			int seqnum_valid = recv_window_validate_seqnum(connection->receive_window, tcp_seqnum(tcp_packet), 0);
			if(seqnum_valid<0){
				printf("connection on socket %d received packet with invalid sequence number\n", connection->socket_id);
				/* If an incoming segment is not acceptable, an acknowledgment
				should be sent in reply (unless the RST bit is set, if so drop
				the segment and return) */
				if(!tcp_rst_bit(tcp_packet)){
					tcp_wrap_packet_send(connection, tcp_header_init(0), NULL, 0);
				}	
				tcp_packet_data_destroy(&tcp_packet_data);
				return;		
			}
		}
		/* lets get the window size first  -- us, not RFC */
		send_window_set_size(connection->send_window, tcp_window_size(tcp_packet));
		
		/* second check the RST bit */
		if(tcp_rst_bit(tcp_packet)){
			/* SYN-RECEIVED STATE */
			if(state == SYN_RECEIVED){
				/* If the RST bit is set: If this connection was initiated with a passive OPEN 
				(i.e., came from the LISTEN state), then return this connection to
				  LISTEN state and return.  The user need not be informed.  If
				  this connection was initiated with an active OPEN (i.e., came
				  from SYN-SENT state) then the connection was refused, signal
				  the user "connection refused".  In either case, all segments
				  on the retransmission queue should be removed.  And in the
				  active OPEN case, enter the CLOSED state and delete the TCB,
				  and return. */
				state_machine_transition(connection->state_machine, receiveRST);
			}
			/* ESTABLISHED, FIN-WAIT-1, FIN-WAIT-2, CLOSE-WAIT */
			else if(state==ESTABLISHED||state==FIN_WAIT_1||state==FIN_WAIT_2||state==CLOSE_WAIT){
				/* If the RST bit is set then, any outstanding RECEIVEs and SEND
				should receive "reset" responses.  All segment queues should be
				flushed.  Users should also receive an unsolicited general
				"connection reset" signal.  Enter the CLOSED state, delete the
				TCB, and return. */
				printf("[Socket %d]: Connection Reset\n", connection->socket_id);
				state_machine_transition(connection->state_machine, receiveRST);
				// ##TODO: how can we delete ourselves??? it has to be api right??
				// tcp_node_remove_connection_kernal(connection->tcp_node, connection);
			}
			/* CLOSING STATE, LAST-ACK STATE, TIME-WAIT */
			else if(state==CLOSING||state==LAST_ACK||state==TIME_WAIT){
				/* If the RST bit is set then, enter the CLOSED state, delete the TCB, and return. */
				state_machine_transition(connection->state_machine, receiveRST);
				// ##TODO: does api handle deleting TCB?
			}
			tcp_packet_data_destroy(&tcp_packet_data);
    		return;			
		}
		// done handling if rst set
		/* third check security and precedence --- WE DON'T DO THIS */
		/* fourth, check the SYN bit, */
		if(tcp_syn_bit(tcp_packet)){
		    /* SYN-RECEIVED, ESTABLISHED STATE, FIN-WAIT STATE-1, FIN-WAIT STATE-2, CLOSE-WAIT STATE
      			CLOSING STATE, LAST-ACK STATE, TIME-WAIT STATE */
      		if(state==SYN_RECEIVED||state==ESTABLISHED||state==FIN_WAIT_1||
      			state==FIN_WAIT_2||state==CLOSE_WAIT||state==CLOSING||state==LAST_ACK||state==TIME_WAIT){
				/* If the SYN is in the window it is an error, send a reset, any
				outstanding RECEIVEs and SEND should receive "reset" responses,
				all segment queues should be flushed, the user should also
				receive an unsolicited general "connection reset" signal, enter
				the CLOSED state, delete the TCB, and return.
		
				If the SYN is not in the window this step would not be reached
				and an ack would have been sent in the first step (sequence
				number check).*/
				printf("[Socket %d]: Connection Reset\n", connection->socket_id);
				state_machine_transition(connection->state_machine, receiveRST);
				tcp_packet_data_destroy(&tcp_packet_data);
    			return;	
			}
		}
		// done handling syn bit
		/*fifth check the ACK field, */
		if(!tcp_ack_bit(tcp_packet)){
		 	/* if the ACK bit is off drop the segment and return */
		 	print(("Socket %d Received packet without ack -- dropping packet", connection->socket_id), PACKET_PRINT);
			tcp_packet_data_destroy(&tcp_packet_data);
    		return;		
    	}
    	else{	/* if the ACK bit is on */
			/* SYN-RECEIVED STATE */
			if(state==SYN_RECEIVED){
				/* If SND.UNA =< SEG.ACK =< SND.NXT then enter ESTABLISHED state and continue processing. */
				if(!_validate_ack(connection, tcp_ack(tcp_packet))){
		 			/* we set the seqnum here because to get this syn-ack we sent our ack with a seqnum 
		 			but no data so our send-window has no way of advancing its seqnums but the 
					receiving end expects the next seqnum */
					send_window_set_seq(connection->send_window, tcp_ack(tcp_packet_data->packet));		
					state_machine_transition(connection->state_machine, receiveACK);	// ack valid --> transition
				}
				else{
					/* If the segment acknowledgment is not acceptable, form a reset segment,
						 <SEQ=SEG.ACK><CTL=RST> and send it. */
		 			print(("Socket %d Received packet with invalid ack -- sending reset", connection->socket_id), PACKET_PRINT);					
					tcp_connection_refuse_connection(connection, tcp_packet_data); //send reset
					tcp_packet_data_destroy(&tcp_packet_data);
    				return;	
    			}
    		}
    		/* ESTABLISHED STATE */
    		else if(state==ESTABLISHED){
				/* see page 72, RFC 793 for the specifics */
				if(!_validate_ack(connection, tcp_ack(tcp_packet))){
					send_window_ack(connection->send_window, tcp_ack(tcp_packet));
					//send next chunks of data
					if(tcp_connection_send_next(connection) > 0)
						update_peer = 0;
				}
			}
			/* FIN-WAIT-1 STATE */
			else if(state==FIN_WAIT_1){
			  /* In addition to the processing for the ESTABLISHED state, if
			  	our FIN is now acknowledged then enter FIN-WAIT-2 and continue
			  	processing in that state.	*/	
			  	
				if(tcp_ack(tcp_packet) == (connection->fin_seqnum)+1){
					// we received the ack for our FIN!
					state_machine_transition(connection->state_machine, receiveACK);
	
			  		/* I don't think our send-window will understand if we ack the ack for the fin, but we 
			  			still need to know that all the previous data has just been acked, so lets ack up to it */
			  		send_window_ack(connection->send_window, tcp_ack(tcp_packet)-1);
			  		/* NOTE: We don't exit because this may have been an ack+fin so then below
			  			we can check the fin and move from FIN_WAIT_2 to TIME_WAIT */ 			  						  		
			  	}
			  	else{
			  		/* just a normal data ack then */		  	
					send_window_ack(connection->send_window, tcp_ack(tcp_packet));
					//send next chunks of data
					tcp_connection_send_next(connection);
				}			
			}
			/* FIN-WAIT-2 STATE */
			else if(state==FIN_WAIT_2){			
				
				/* In addition to the processing for the ESTABLISHED state,*/
				send_window_ack(connection->send_window, tcp_ack(tcp_packet));
				//send next chunks of data
				if(tcp_connection_send_next(connection) > 0)
					update_peer = 0;
							
				/* if the retransmission queue is empty, the user's CLOSE can be
          			acknowledged ("ok") but do not delete the TCB.	*/
          			//I'M CONFUSED
          	}
          	/* CLOSE-WAIT STATE--We're just waiting for our user to finally close after receiving unsolicited FIN*/
			else if(state==CLOSE_WAIT){				
          		/* Do the same processing as for the ESTABLISHED state. */
				send_window_ack(connection->send_window, tcp_ack(tcp_packet));
				//send next chunks of data
				if(tcp_connection_send_next(connection) > 0)
					update_peer = 0;
			}
			/* CLOSING STATE */
			else if(state == CLOSING){
			  	/* In addition to the processing for the ESTABLISHED state, */
				send_window_ack(connection->send_window, tcp_ack(tcp_packet));
				//send next chunks of data
				if(tcp_connection_send_next(connection) > 0)
					update_peer = 0;
							  
			  	/* if the ACK acknowledges our FIN then enter the TIME-WAIT state,
			  	otherwise ignore the segment.		*/
				if(tcp_ack(tcp_packet) == (connection->fin_seqnum)+1)
					// we received the ack for our FIN!
					state_machine_transition(connection->state_machine, receiveACK);				
							  		
			}
			/* LAST-ACK STATE */
			else if(state == LAST_ACK){
				/* The only thing that can arrive in this state is an
				acknowledgment of our FIN.  If our FIN is now acknowledged,
				delete the TCB, enter the CLOSED state, and return. */
				if(tcp_ack(tcp_packet) == (connection->fin_seqnum)+1){
					// we received the ack for our FIN!
					state_machine_transition(connection->state_machine, receiveACK);				
					tcp_packet_data_destroy(&tcp_packet_data);
    				return;
				}
			}
			/* TIME-WAIT STATE */
			else if(state == TIME_WAIT){
			  /* The only thing that can arrive in this state is a
			  retransmission of the remote FIN.  Acknowledge it, and restart
			  the 2 MSL timeout. */		
			  if(tcp_fin_bit(tcp_packet))
			  		state_machine_transition(connection->state_machine, receiveFIN);	
			}
		}
		/* sixth, check the URG bit, -- WE DONT HWAHAHA */
		/* seventh, process the segment text, */
		/*ESTABLISHED STATE, FIN-WAIT-1 STATE, FIN-WAIT-2 STATE */
		if(state == ESTABLISHED || state == FIN_WAIT_1 || state == FIN_WAIT_2){
			/* Once in the ESTABLISHED state, it is possible to deliver segment
        		text to user RECEIVE buffers.  blah blah etc see page 74 */
     			/* DATA check if there's any data, and if there is push it to the window */
     			/* If user used the shutdown r option, then we'll just let them know in our ack that our
     				window size is 0.  All good! */
			memchunk_t data = tcp_unwrap_data(tcp_packet, tcp_packet_data->packet_size);
			if(data){ 
				recv_window_receive(connection->receive_window, data->data, data->length, tcp_seqnum(tcp_packet));
				memchunk_destroy(&data);
				// if there's a blocking read, need to signal we got more data to read
				if(connection->recv_window_alive)
					tcp_connection_api_signal(connection, 1);
				// now lets send our ack -- handles the update peer situation
				/* now update that peer because friends don't let friends send unacknowledged bytes*/
				tcp_wrap_packet_send(connection, tcp_header_init(0), NULL, 0);
			}   	
        }
       	/* CLOSE-WAIT STATE, CLOSING STATE, LAST-ACK STATE, TIME-WAIT STATE
			This should not occur, since a FIN has been received from the
			remote side.  Ignore the segment text.   */ 
		
		/* eighth, check the FIN bit, */
		if(tcp_fin_bit(tcp_packet)){
			/* Do not process the FIN if the state is CLOSED, LISTEN or SYN-SENT
     		 since the SEG.SEQ cannot be validated; drop the segment and return. */
     		 if(state == CLOSED || state == LISTEN || state == SYN_SENT){  //<-- we will never actually get here
  				tcp_packet_data_destroy(&tcp_packet_data);
    			return;
    		}
    		else{   		 	
				 /* If the FIN bit is set, signal the user "connection closing" and
				  return any pending RECEIVEs with same message, advance RCV.NXT
				  over the FIN, and send an acknowledgment for the FIN.  Note that
				  FIN implies PUSH for any segment text not yet delivered to the
				  user. */
				 connection->last_seq_received = tcp_seqnum(tcp_packet);
				 state_machine_transition(connection->state_machine, receiveFIN);
			}
		}

    	tcp_packet_data_destroy(&tcp_packet_data);
    	return;	
	}
	
	/* SEE PAGE 64 OF RFC 793: IN THE ABOVE SECTION I FOLLOW IT PRECISELY */
	
////////////////////////////////////////////////////////////////////////////////////////////////
	/* receive window is NULL until we transition to established or SYN_RECEIVED.  It's set NULL again when we 
		stop receiving data, but then we don't care about seqnum's anyhow, right??
		There are certain operations and checks we should make only if the seqnum is valid.
		We certainly shouldn't push to a window that doesn't exist is the other thing 
		
			Times when we should accept an invalid seqnum (I think, says Alex):
			SYN_SENT: because it's possible we're in a simultaneous open
			Listen: we don't really yet have a seqnum
	*/	
// 	if(connection->receive_window != NULL){
// 		int seqnum_valid = recv_window_validate_seqnum(connection->receive_window, tcp_seqnum(tcp_packet), 0);
// 		if(seqnum_valid<0){
// 			printf("connection on socket %d received packet with invalid sequence number\n", connection->socket_id);
// 			//drop packet, right? Should we send an RST in particular cases?
// 			tcp_packet_data_destroy(&tcp_packet_data);
// 			return;
// 		}
// 		/* DATA 
// 			check if there's any data, and if there is push it to the window,
// 			but what does the seqnum even mean if the ACKs haven't been synchronized? */
// 		memchunk_t data = tcp_unwrap_data(tcp_packet, tcp_packet_data->packet_size);
// 		if(data){ 
// 			recv_window_receive(connection->receive_window, data->data, data->length, tcp_seqnum(tcp_packet));
// 			// if there's a blocking read, need to signal we got more data to read
// 			tcp_connection_api_signal(connection, 1);
// 		
// 			/* send the ack back */
// 			memchunk_destroy(&data);
// 	
// 			/* we want to update our peer what we've acked */
// 			update_peer = 1;
// 		}
// 	}
// 	
// 	if(state == LISTEN){
// 		/* since listen binds to all interfaces, must be able to reset its ip addresses to receive connect requests*/
// 		tcp_connection_set_remote(connection, tcp_packet_data->remote_virt_ip, tcp_source_port(tcp_packet));
// 		tcp_connection_set_local_ip(connection, tcp_packet_data->local_virt_ip);
// 	}
// 
// 	/* lets get the window size first */
// 	if(connection->send_window)
// 		send_window_set_size(connection->send_window, tcp_window_size(tcp_packet));
// 
// 	if(tcp_rst_bit(tcp_packet)){
// 	/*  In all states except SYN-SENT, all reset (RST) segments are validated
// 	  by checking their SEQ-fields. (Which we did above!) A reset is valid if its sequence number
// 	  is in the window.  In the SYN-SENT state (a RST received in response
// 	  to an initial SYN), the RST is acceptable if the ACK field
// 	  acknowledges the SYN.*/
// 		if(state == SYN_SENT){
// 			if(_validate_ack(connection, tcp_ack(tcp_packet)) < 0){
// 				print(("Received invalid ack with rst bit while in SYN_SENT state -- discarding packet"), TCP_PRINT);
// 				tcp_packet_data_destroy(&tcp_packet_data);
// 				return;
// 			}
// 		}
// 		/* The receiver of a RST first validates it, then changes state.  If the
// 		  receiver was in the LISTEN state, it ignores it.  If the receiver was
// 		  in SYN-RECEIVED state and had previously been in the LISTEN state,
// 		  then the receiver returns to the LISTEN state, otherwise the receiver
// 		  aborts the connection and goes to the CLOSED state.  If the receiver
// 		  was in any other state, it aborts the connection and advises the user */
// 		  
// 		/* So let's abort the connection now: */
// 		print(("received valid rst"),TCP_PRINT);
// 		state_machine_transition(connection->state_machine, receiveRST);	
// 		update_peer = 0;
// 	
// 	
// 	//tcp_packet_data_destroy(&tcp_packet_data);
// 	}
// 	/* check FIN bit */
// 	else if(tcp_fin_bit(tcp_packet)){
// 		print(("received fin"),TCP_PRINT);
// 		if(tcp_ack_bit(tcp_packet)){
// 			print(("received ack with fin"),TCP_PRINT);
// 			if(!_validate_ack(connection, tcp_ack(tcp_packet))){
// 				if(connection->send_window)
// 					send_window_ack(connection->send_window, tcp_ack(tcp_packet));
// 			}
// 		}
// 		connection->fin_seqnum = tcp_seqnum(tcp_packet);
// 		
// 		tcp_connection_receive_FIN(connection);
// 		update_peer = 0;
// 	}		
// 
// 	/* now check the SYN bit */
// 	else if(tcp_syn_bit(tcp_packet) && tcp_ack_bit(tcp_packet)){
// 		tcp_connection_handle_syn_ack(connection, tcp_packet_data);	
// 		update_peer = 0;
// 	}
// 	
// 	/* ack data if you're in a position to do so */
// 	else if(tcp_ack_bit(tcp_packet)){
// 	
// 	
// 	
//  		if(connection->send_window)
// 			send_window_ack(connection->send_window, tcp_ack(tcp_packet));
// 				
// 		if(state == ESTABLISHED){
// 			//send next chunks of data
// 			if(tcp_connection_send_next(connection) > 0)
// 				update_peer = 0;
// 		}	
// 		
// 		// do we want an else? that was a bad ACK (its not acking anything), or we fucked up
// 		else if(state == SYN_RECEIVED){ //<-- Neil: why did you change that to syn_sent????
// 			/* we set the seqnum here because to get this syn-ack we sent our ack with a seqnum 
// 			but no data so our send-window has no way of advancing its seqnums but the 
// 			receiving end expects the next seqnum */
// 			send_window_set_seq(connection->send_window, tcp_ack(tcp_packet_data->packet));			
// 			state_machine_transition(connection->state_machine, receiveACK);
// 		}	
// 		
// 		// if we sent a FIN we're waiting for its ack	
// 		else if(state == FIN_WAIT_1 || state == CLOSING || state == LAST_ACK){
// 			if(tcp_ack(tcp_packet) == (connection->fin_seqnum)+1)
// 				// we received the ack for our FIN!
// 				state_machine_transition(connection->state_machine, receiveACK);
// 		}
// 	}
// 	
// 	else if(tcp_syn_bit(tcp_packet)){
//         if(state == SYN_SENT){
//             // we think the TA implementation is WRONG and now we have to deal with it WTF
//             tcp_connection_handle_syn_ack(connection, tcp_packet_data);
//             update_peer = 0;
//         }
//         else{
//             tcp_connection_handle_syn(connection, tcp_packet_data);
//             update_peer = 0;
//         }
// 	}
// 
// 	/* now update that peer because friends don't let friends send unacknowledged bytes*/
// 	if(update_peer)
// 		tcp_wrap_packet_send(connection, tcp_header_init(0), NULL, 0);
// 		
// 	/* Clean up */
// 	tcp_packet_data_destroy(&tcp_packet_data);
}

void tcp_connection_handle_syn_ack(tcp_connection_t connection, tcp_packet_data_t tcp_packet_data){

	/* we set the seqnum here because to get this syn-ack we sent our ack with a seqnum 
		but no data so our send-window has no way of advancing its seqnums but the 
		receiving end expects the next seqnum */
	send_window_set_seq(connection->send_window, tcp_ack(tcp_packet_data->packet));
   
	print(("syn/ack"),TCP_PRINT);

	void* tcp_packet = tcp_packet_data->packet;

	if(_validate_ack(connection, tcp_ack(tcp_packet)) < 0){
		/* then you sent a syn with a seqnum that wasn't faithfully returned. 
			what should we do? for now, let's discard */
		puts("Received invalid ack with SYN/ACK. Discarding.");
		return;
	}
	
	/* we need to reset the port that we're using on the remote 
		machine in order to match the connection that accepted us */
	connection->remote_addr.virt_port = tcp_source_port(tcp_packet_data->packet);

	/* received a SYN/ACK, record the seq you got, and validate
		that the ACK you received is correct */
	connection->last_seq_received = tcp_seqnum(tcp_packet);
	
	// this function calls tcp_connection_api_finish if in SYN_SENT
	state_machine_transition(connection->state_machine, receiveSYN_ACK); 
}

void tcp_connection_handle_syn(tcp_connection_t connection, tcp_packet_data_t tcp_packet_data){
	print(("syn"),TCP_PRINT);

	void* tcp_packet = tcp_packet_data->packet;

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
		tcp_connection_handle_syn_LISTEN(connection, tcp_connection_get_local_ip(connection),
									tcp_connection_get_remote_ip(connection), 
									tcp_source_port(tcp_packet), 
									tcp_seqnum(tcp_packet));
		// anything else??		
	}
	else
		// otherwise refuse the connection 
		tcp_connection_refuse_connection(connection, tcp_packet_data);
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
	print(("queueing"), TCP_PRINT);

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
	
	/* Record last seq sent -- only if we purposely set the seqnum */
	/* NO BECAUSE WE RESEND UN-ACKED PACKETS
	if(data_len > 0 || tcp_syn_bit(header))
		connection->last_seq_sent = tcp_seqnum(header); */
	
	// gotta put a seqnum on it, right?	
	if((data_len == 0) && (!tcp_seqnum(header))){
		puts("tcp_wrap_packet_send 0");
		if(connection->send_window)
			tcp_set_seq(header, send_window_get_next_seq(connection->send_window));
		else
			tcp_set_seq(header, (connection->last_seq_sent)+1); //increment by 1 right??
	}
	
	/* PORTS */
	tcp_set_dest_port(header, connection->remote_addr.virt_port);
	tcp_set_source_port(header, connection->local_addr.virt_port);

	/* WINDOW SIZE */
	tcp_set_window_size(header, DEFAULT_WINDOW_SIZE);
	if(connection->receive_window) //we don't set it until we receive our first byte
		tcp_set_window_size(header, recv_window_get_size(connection->receive_window));
	if(!connection->recv_window_alive)
		/* means we closed the receive window down 
			-- so lets practice congestion control and tell them not to send more data */
		tcp_set_window_size(header, 0);

	
	/* ACK */
	if(!tcp_ack(header)){
		/* We may have already set it -- like if we were trying to ack a fin -- so don't reset */
		
		if(connection->receive_window){
			tcp_set_ack_bit(header);
			tcp_set_ack(header, recv_window_get_ack(connection->receive_window));
		}
		// if in one of closing states, possible we already closed receive window but still need to correctly ack
		// for now we're doing it a slightly hacky way of setting it to last_seq_received + 1
		else if(tcp_connection_in_closing_state(connection)){
			tcp_set_ack_bit(header);
			tcp_set_ack(header, (connection->last_seq_received)+1);	
		}
	}
	
	/* DATA */
	uint32_t total_length = tcp_offset_in_bytes(header) + data_len;
    
    /* print it */
    if(tcp_connection_get_state(connection) != ESTABLISHED){
    	print(("Sending Packet of length %u", total_length), PACKET_PRINT);
    	view_packet(header, data, data_len); 
    }   
    
	if((data != NULL)&&(data_len)){
		memcpy(((char*)header)+tcp_offset_in_bytes(header), data, data_len);
		free(data);
	}
	
	uint32_t local_ip = connection->local_addr.virt_ip, 
			remote_ip = connection->remote_addr.virt_ip;
	
	/* CHECKSUM */
	tcp_utils_add_checksum(header, total_length, local_ip, remote_ip, TCP_DATA);
	
	/* init the packet */
	tcp_packet_data_t packet_data = tcp_packet_data_init(
										(char*)header, 
										total_length,
										local_ip, remote_ip);

	/* queue it */
	if(tcp_connection_queue_ip_send(connection, packet_data) < 0){
		//TODO: HANDLE!
		puts("Something wrong with sending tcp_packet to_send queue--How do we want to handle this??");	
		free(packet_data);
		return -1;
	}

	return 1;
}

void tcp_connection_send_next_chunk(tcp_connection_t connection, send_window_chunk_t next_chunk){
	// mallocs enough memory for the header and the data
	struct tcphdr* header = tcp_header_init(next_chunk->length);
		
	/* the seqnum should be the seqnum in the next_chunk */
	tcp_set_seq(header, next_chunk->seqnum);
	
	/* send it off! */
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
		CRASH_AND_BURN("Sending window null when trying to push data");
	
	// memcpys the data into the window (will handle freeing the data)
	send_window_push(connection->send_window, data, data_len);
}

// queues chunks off from send_window and handles sending them for as long as send_window wants to send more chunks
int tcp_connection_send_next(tcp_connection_t connection){
	int bytes_sent = 0;
	send_window_chunk_t next_chunk;
	send_window_t send_window = connection->send_window;

	// keep sending as many chunks as window has available to give us
	// get_next gives you a copy of the data in the window
	while((next_chunk = send_window_get_next(send_window))){
	
		// send it off
		tcp_connection_send_next_chunk(connection, next_chunk);

		// increment bytes_sent
		bytes_sent += next_chunk->length;

		send_window_chunk_destroy(&next_chunk);
	}	
	return bytes_sent;
}

int tcp_connection_send_data(tcp_connection_t connection, const unsigned char* to_write, int num_bytes){
	
	state_e state = tcp_connection_get_state(connection);
	if(!(state == ESTABLISHED || state == CLOSE_WAIT)){
		puts("Trying to send data on a non-established connection.");
		return -EINVAL; // whats the correct error code here?
	}

	// push data to window and then send as much as we can
	tcp_connection_push_data(connection, (void*)to_write, num_bytes);	

	// send as much data right now as send_window allows
	int ret = tcp_connection_send_next(connection);
	
	return ret;
}


/********************* End of Sending Packets ********************/
  
uint16_t
tcp_connection_get_local_port(tcp_connection_t connection){ return connection->local_addr.virt_port; }

uint16_t
tcp_connection_get_remote_port(tcp_connection_t connection){ return connection->remote_addr.virt_port; }

void
tcp_connection_set_local_port(tcp_connection_t connection, uint16_t port){ connection->local_addr.virt_port = port; }

uint32_t
tcp_connection_get_local_ip(tcp_connection_t connection){ return connection->local_addr.virt_ip; }

uint32_t 
tcp_connection_get_remote_ip(tcp_connection_t connection){ return connection->remote_addr.virt_ip; }

int 
tcp_connection_get_socket(tcp_connection_t connection){	return connection->socket_id; }

/*0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0 to_read Thread o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0o0*/

// runs thread for tcp_connection to handle sending/reading and keeping track of its packets/acks
void *_handle_read_send(void *tcpconnection){
	
	tcp_connection_t connection = (tcp_connection_t)tcpconnection;

	struct timespec wait_cond;	
	struct timeval now;	// keep track of time to compare to window timeouts and connections' syn_timer 
	float time_elapsed;
	void* packet;
	int ret, timers_ret;

	while(connection->running){	
        
        state_e state = tcp_connection_get_state(connection);
        
		gettimeofday(&now, NULL);	
		wait_cond.tv_sec = now.tv_sec+0;
		wait_cond.tv_nsec = 1000*now.tv_usec+TCP_CONNECTION_DEQUEUE_TIMEOUT_NSECS;

        wait_cond.tv_sec += wait_cond.tv_nsec/1000000000;
        wait_cond.tv_nsec %= 1000000000;
        
		ret = bqueue_timed_dequeue_abs(connection->my_to_read, &packet, &wait_cond);

		time_elapsed = now.tv_sec - connection->state_timer.tv_sec;
		time_elapsed += now.tv_usec/1000000.0 - connection->state_timer.tv_usec/1000000.0;
		
		if(state == ESTABLISHED){
			//let's send a keep-alive message
			if(time_elapsed > KEEP_ALIVE_FREQUENCY)
				tcp_connection_send_keep_alive(connection);
		}
		/* check if you're waiting for an ACK to come back */
		else if(state == SYN_SENT){	         
			if(time_elapsed > (1 << ((connection->syn_fin_count)-1))*RETRANSMISSION_TIMEOUT){
				// we timeout connect or resend

				if((connection->syn_fin_count)==SYN_COUNT_MAX){
					// timeout connection attempt
					connection->syn_fin_count = 0;
					tcp_connection_state_machine_transition(connection, CLOSE); 
				}
				else{	
					// resend syn
					tcp_connection_send_syn(connection); //this call increments syn_fin_count
				}
			}
		}
		else if(state == SYN_RECEIVED){
			/* after a conservative amount of time, let's let the SYN_RECEIVED time out so that accept doesn't 
				block waiting for the api signal forever */
			if(time_elapsed > (1 << 3)*RETRANSMISSION_TIMEOUT){
				// right now we're letting the api close and remove it when it sees this timeout
				tcp_connection_api_signal(connection, API_TIMEOUT);
			}
		}
	/************************ Handle in PASSIVE CLOSING states ******************************/
		else if(state == LAST_ACK){
			/* If an ACK is not forthcoming, after the user timeout the connection is aborted and the user is told. */
			if(time_elapsed > USER_TIMEOUT){
				state_machine_transition(connection->state_machine, ABORT);
			}
		}
				
	/************************ Handle in ACTIVE CLOSING states ******************************/
		else if(state == FIN_WAIT_1){
			/* For active close: 
				RFC: All segments preceding and including FIN  will be retransmitted until acknowledged. */
			if(time_elapsed > WINDOW_DEFAULT_TIMEOUT){ // lets use same timeout as send_window
				if((connection->syn_fin_count)==SYN_COUNT_MAX){
					// lets give up on them ever acking it
					tcp_connection_ABORT(connection);
				}
				else
					tcp_connection_send_fin(connection);	
			}
		}
		else if(state == TIME_WAIT){
			/* For active close: wait for 2 MSL before transitioning to CLOSED */
			if(time_elapsed > 2*MSL){
				/* TIME_WAIT_to_CLOSED transition will signal api that TCB can be deleted */
				state_machine_transition(connection->state_machine, TIME_ELAPSED);
			}	
		}
	/************************* DONE CHECKING CLOSING STATE NEEDS *******************************/	
		/* send whatever you're trying to send */
		if(connection->send_window){ /*<-- important to check if this still is a thing because we may have 
											destroyed it in the loop above when we transitioned to CLOSED */
			timers_ret = send_window_check_timers(connection->send_window);
			tcp_connection_send_next(connection);
		}
		/* If we're in certain closing states, after all of our data reliably sent (acked) AND fin acked
			then we can proceed with rest of close process */
		
		//if in CLOSING, can't ack fin until received ack for every preceding segment
		if((!timers_ret) && (state == CLOSING))
			tcp_connection_ack_fin(connection); 
		
		/* now check if there's something to read */
		if (ret != 0) 
			/* should probably check at this point WHY we failed (for instance perhaps the queue
				was destroyed */
            // should be EINVAL(ask alex about that linux issue we found and she victoriously debugged) or ETIMEDOUT
			continue;

		//handle to read packet
		tcp_connection_handle_receive_packet(connection, packet);
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

	accept_queue_data_t data = NULL;
	while(!bqueue_trydequeue(q, (void**)&data)){
		accept_queue_data_destroy(&data);
	}
	bqueue_destroy(q);
	free(q);
	connection->accept_queue = NULL;
}


// return popped triple -- null if error in dequeue
accept_queue_data_t tcp_connection_accept_queue_dequeue(tcp_connection_t connection){
	bqueue_t *q = connection->accept_queue;
	if(q==NULL){
		puts("Error in tcp_connection_accept_queue_dequeue: SEE CODE");
		return NULL;
	}
    struct timespec wait_cond;  
    struct timeval now; 

    accept_queue_data_t data;
    int ret;

    while((!(connection->closing))&&(connection->running)&&(tcp_node_running(connection->tcp_node))){ 
        
        if(tcp_connection_get_state(connection) != LISTEN){ //this might be redundant with the closing boolean
        	// we should only be dequeuing like this if we're in the LISTEN state 
        	return NULL;
        }
        
        gettimeofday(&now, NULL);   
        wait_cond.tv_sec = now.tv_sec+0;
        wait_cond.tv_nsec = (1000*now.tv_usec+TCP_CONNECTION_DEQUEUE_TIMEOUT_NSECS);
        
        wait_cond.tv_sec += wait_cond.tv_nsec/1000000000;
        wait_cond.tv_nsec %= 1000000000;
    
        ret = bqueue_timed_dequeue_abs(q, (void*)&data, &wait_cond);
        
        if(ret!= -ETIMEDOUT)
            break;
    }
    if((ret != 0) || (tcp_connection_get_state(connection) != LISTEN) || (connection->closing))
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
	//tcp_connection_print_state(connection);
	int ret = state_machine_transition(connection->state_machine, transition);
	//tcp_connection_print_state(connection);
	return ret;
}

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

/* signaling */

/* If waiting for api signal and trying to shutdown, we end up blocking -- need way out
	to be called before pthread_destroy on api thread
	sets ret to SIGNAL_DESTROYING --should be something else? */
void tcp_connection_api_cancel(tcp_connection_t connection){
	tcp_connection_api_signal(connection, SIGNAL_DESTROYING);
}
	
/* calls pthread_cond_signal(api_cond) so that the waiting tcp_api function can stop waiting and take a look at the return value		
tcp_connection_api_signal(connection); 
*/
void tcp_connection_api_signal(tcp_connection_t connection, int ret){
	/* set return value and signal that tcp_api function finished on the connection's part */
	connection->api_ret = ret;
	pthread_cond_signal(&(connection->api_cond));
}

void tcp_connection_api_lock(tcp_connection_t connection){
	pthread_mutex_lock(&(connection->api_mutex));
}

void tcp_connection_api_unlock(tcp_connection_t connection){
	pthread_mutex_unlock(&(connection->api_mutex));
}

int tcp_connection_api_result(tcp_connection_t connection){
	int wait = 1;
	
	struct timespec ts;
	struct timeval tv;

	while(1){
		//puts("waiting on condition");

		gettimeofday(&tv, NULL);	
		ts.tv_sec = tv.tv_sec + wait;
		ts.tv_nsec = tv.tv_usec*1000;

	 	int result = pthread_cond_timedwait(&(connection->api_cond), &(connection->api_mutex), &ts);
		if(result==0)
			break;
		if(result<0 && result!=ETIMEDOUT){
			printf("%d\n", result);
			CRASH_AND_BURN("cond_wait failed for api_result");
		}
		// otherwise you timedout, keep waiting
	}
	
	// let's see if this fixes the blocking issue
	//tcp_connection_api_unlock(connection);

	int ret = connection->api_ret;
	if(ret == SIGNAL_CRASH_AND_BURN)
		CRASH_AND_BURN("Received crash_and_burn from the connection result");
	
	else if(ret == SIGNAL_DESTROYING)
		return SIGNAL_DESTROYING;
		
	else 
		return ret;
}
// sometimes we just need to give up.  eg ABORT transition called in thread after fin never acked
/* send rst and transition to closed by ABORT */
int tcp_connection_ABORT(tcp_connection_t connection){
	
	/* We're going into CLOSED state */
	connection->closing = 1;
	
	connection->syn_fin_count = 0;
	
	state_e state = state_machine_get_state(connection->state_machine);
	// if state one of the following, RFC says we should send rst
	if(state==SYN_RECEIVED || state==ESTABLISHED || state==FIN_WAIT_1 || state==FIN_WAIT_2 || state == CLOSE_WAIT){
 		//send rst
		tcp_connection_refuse_connection(connection, NULL);
	}
	
	// transition
	state_machine_transition(connection->state_machine, ABORT);

	if(connection->receive_window)
		recv_window_destroy(&(connection->receive_window));
	
	printf("[Socket %d]: Connection Aborted\n", connection->socket_id);
	tcp_connection_api_signal(connection, -ETIMEDOUT);
	return 1;	
}

/* refuse the connection (send a RST) */
void tcp_connection_refuse_connection(tcp_connection_t connection, tcp_packet_data_t packet){
 	
 	struct tcphdr* outgoing_header;
 	
 	/* We also want to be able to use this when instead of receiving a bad packet, we want our
 		connection to abort - so we send an RST */
 	if(packet == NULL){
		outgoing_header = tcp_header_init(0); 		

		/* PORTS */
		tcp_set_dest_port(outgoing_header, connection->remote_addr.virt_port);
		tcp_set_source_port(outgoing_header, connection->local_addr.virt_port);

		/* SEQNUM */
		tcp_set_seq(outgoing_header, send_window_get_next_seq(connection->send_window));
		
		/* ACK  not specified by RST for abort call */
	}
	else{			
		/*  RFC 793: pg 35
		If the connection does not exist (CLOSED) then a reset is sent
		in response to any incoming segment except another reset.	****
		In particular, SYNs addressed to a non-existent connection are rejected
		by this means. */  
		struct tcphdr* incoming_header = (struct tcphdr*)packet->packet;
	
		if(tcp_rst_bit(incoming_header))
			return; //by **** a few lines right above
	
		outgoing_header = tcp_header_init(0);
	
		/* PORTS */
		tcp_set_dest_port(outgoing_header, tcp_source_port(incoming_header));
		tcp_set_source_port(outgoing_header, tcp_dest_port(incoming_header));
	
		/* SEQNUM */
			/*If the incoming segment has an ACK field, the reset takes its
			sequence number from the ACK field of the segment, otherwise the
			reset has sequence number zero and the ACK field is set to the sum
			of the sequence number and segment length of the incoming segment.
			The connection remains in the CLOSED state.*/
		if(tcp_ack_bit(incoming_header)){
			tcp_set_seq(outgoing_header, tcp_ack(incoming_header));
		}
		else{
			tcp_set_seq(outgoing_header, 0);
			/* ACK */
			int seg_length = packet->packet_size - tcp_offset_in_bytes(incoming_header) + 1;
			tcp_set_ack(outgoing_header, (tcp_seqnum(incoming_header)+seg_length));
			tcp_set_ack_bit(outgoing_header);
		}
	}


	/* RST */
	tcp_set_rst_bit(outgoing_header);

	/* CHECKSUM */
	tcp_utils_add_checksum(outgoing_header, tcp_offset_in_bytes(outgoing_header), connection->local_addr.virt_ip, connection->remote_addr.virt_ip, TCP_DATA);

	tcp_packet_data_t packet_data = tcp_packet_data_init(
										(char*)outgoing_header, 
										tcp_offset_in_bytes(outgoing_header)+0,
										connection->local_addr.virt_ip,
										connection->remote_addr.virt_ip);

	if(tcp_connection_queue_ip_send(connection, packet_data) < 0){
		puts("Something wrong with sending tcp_packet to_send queue--How do we want to handle this??");	
		free(packet_data);
	}

}

//needed for tcp_api_read and driver window_cmd
recv_window_t tcp_connection_get_recv_window(tcp_connection_t connection){
	return connection->receive_window;
}
// needed for driver window_cmd
send_window_t tcp_connection_get_send_window(tcp_connection_t connection){
	return connection->send_window;
}
// returns 1=true if connection has reading capabilities, 0=false otherwise
int tcp_connection_recv_window_alive(tcp_connection_t connection){
	return connection->recv_window_alive;
}

/* Instead of destroying receive window we just keep track of whether or not it exists.
	We don't want to destroy it because we still need it to catch the data sent and keep track of sequence numbers
	in case we shut down the receive window while still in the established state
	 this is necessary for api call v_shutdown type 2 when we just need to close the reading portion of the connection
	 returns 1 on success, -1 on failure */
int tcp_connection_close_recv_window(tcp_connection_t connection){
	if(!connection->recv_window_alive)
		return -1;
	connection->recv_window_alive = 0;
	return 1;
}



/* hacky? */  //<-- yeah kinda
#include "tcp_connection_state_machine_handle.c"

		/*

	* this will almost universally be true. *
	else if(tcp_ack_bit(tcp_packet)){
		print(("ack."),TCP_PRINT);

		if(_validate_ack(connection, tcp_ack(tcp_packet)) < 0){
			// should we drop the packet here?
			puts("Received invalid ack with ACK. Discarding.");
			return;
		}
		
		//reset remote ip/port in case it has changed
		tcp_connection_set_remote(connection, tcp_packet_data->remote_virt_ip, tcp_source_port(tcp_packet));
		
		//TODO: *************** todo *********************
		
		* careful, this might be NULL -- shuldn't we actually do this in the state_machine transition?*
		//send_window_ack(connection->send_window, tcp_ack(tcp_packet));
		
		// should we give this ack to the receive window?
		
		state_machine_transition(connection->state_machine, receiveACK);
	}

	*/
