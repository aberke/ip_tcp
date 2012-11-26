#ifndef __WINDOW_H__ 
#define __WINDOW_H__

#include "utils.h"
#include <inttypes.h>
#include <sys/time.h>
#include <time.h>

/* RFC:
   An Example Retransmission Timeout Procedure

      Measure the elapsed time between sending a data octet with a
      particular sequence number and receiving an acknowledgment that
      covers that sequence number (segments sent do not have to match
      segments received).  This measured elapsed time is the Round Trip
      Time (RTT).  Next compute a Smoothed Round Trip Time (SRTT) as:

        SRTT = ( ALPHA * SRTT ) + ((1-ALPHA) * RTT)

      and based on this, compute the retransmission timeout (RTO) as:

        RTO = min[UBOUND,max[LBOUND,(BETA*SRTT)]]

      where UBOUND is an upper bound on the timeout (e.g., 1 minute),
      LBOUND is a lower bound on the timeout (e.g., 1 second), ALPHA is
      a smoothing factor (e.g., .8 to .9), and BETA is a delay variance
      factor (e.g., 1.3 to 2.0).
*/
#define WINDOW_ALPHA 0.8
#define WINDOW_BETA 1.5
#define WINDOW_UBOUND 10 //10 seconds is the upper bound on the window RTO -- really long, right?
#define WINDOW_LBOUND 0.001 //1 millisecond

#define WINDOW_CHUNK_SIZE 1024
#define MAX_SEQNUM ((unsigned)-1)

typedef struct send_window* send_window_t;

/* RFC:     An Example Retransmission Timeout Procedure

      Measure the elapsed time between sending a data octet with a
      particular sequence number and receiving an acknowledgment that
      covers that sequence number (segments sent do not have to match
      segments received).  This measured elapsed time is the Round Trip
      Time (RTT).  Next compute a Smoothed Round Trip Time (SRTT) as:

        SRTT = ( ALPHA * SRTT ) + ((1-ALPHA) * RTT)

      and based on this, compute the retransmission timeout (RTO) as:

        RTO = min[UBOUND,max[LBOUND,(BETA*SRTT)]]

      where UBOUND is an upper bound on the timeout (e.g., 1 minute),
      LBOUND is a lower bound on the timeout (e.g., 1 second), ALPHA is
      a smoothing factor (e.g., .8 to .9), and BETA is a delay variance
      factor (e.g., 1.3 to 2.0). */
struct send_window_chunk{
	struct timeval send_time;
	void* data;
	int length;
	int seqnum;
	int offset;
	int resent; /*number of times chunk has been resent so we can practice exponential backoff.  This will also
					allow us to avoid effecting RTO incorrectly.  resent initialized at 0 */
};

typedef struct send_window_chunk* send_window_chunk_t;

void send_window_chunk_destroy(send_window_chunk_t* wc);
void send_window_chunk_destroy_total(send_window_chunk_t* wc, destructor_f destructor);
void send_window_chunk_destroy_free(send_window_chunk_t* wc);

void send_window_set_seq(send_window_t sc, uint32_t seq);

send_window_t send_window_init(int window_size, int send_size, int ISN, 
								double ALPHA, double BETA, double UBOUND, double LBOUND);
void send_window_destroy(send_window_t* send_window);

// use syn-timeout same as send_window RTO -- so we need to get it
double send_window_get_RTO(send_window_t send_window);
void send_window_set_size(send_window_t send_window, uint32_t size);
void send_window_push(send_window_t send_window, void* data, int length);
/* Alex wants to be able to use this for closing purposes as well
	-- so if there are no more timers to check -- all data sent successfully acked 
	-- then we cant continue with close
	returns 0 if no more outstanding segmements -- all data sent acked
	returns > 0 number for remaining outstanding segments 
	NEIL -- PLEASE CHECK THAT I DID THIS RIGHT */
int send_window_check_timers(send_window_t send_window);
int send_window_validate_ack(send_window_t send_window, uint32_t ack);
void send_window_ack(send_window_t send_window, int index);
void send_window_resize(send_window_t send_window, int size);
uint32_t send_window_get_next_seq(send_window_t send_window);
send_window_chunk_t send_window_get_next(send_window_t send_window);

// needed for driver window_cmd
int send_window_get_size(send_window_t send_window);


void send_window_print(send_window_t send_window);

#endif // __WINDOW_H__
