#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <time.h>
#include <sys/select.h>
#include "tcp_utils.h"

/* ALL THE TIMEOUTS */
#define TCP_CONNECTION_DEQUEUE_TIMEOUT_NSECS 10000000

/* for printing */
#define IP_PRINT 			1
#define TCP_PRINT 			2
#define WINDOW_PRINT 		3
#define SEND_WINDOW_PRINT 	4
#define LEAK_PRINT 			5
#define STATES_PRINT 		6
#define CLOSING_PRINT		7
#define PACKET_PRINT	  	8

#define ALEX_PRINT			9 //yup I did it.
//#define ALEXS_SEGFAULT		10 --she found it!  It was that plainlist all along (NOT Alex's fault whoo)
#define PORT_PRINT			10
#define CHECKSUM_PRINT		11

#define mask (  0									\
		  /* | (1<<(IP_PRINT-1))		*/    		\
		  /* | (1<<(TCP_PRINT-1)) 		*/    		\
		  /* | (1<<(WINDOW_PRINT-1)) 	*/			\
	   	  /*   | (1<<(SEND_WINDOW_PRINT-1))	 */	\
		   /*| (1<<(LEAK_PRINT-1))          */		\
		   /*| (1<<(STATES_PRINT-1))		*/		\
		   /* | (1<<(CLOSING_PRINT-1))	 	*/	    \
		   /* | (1<<(PACKET_PRINT-1))		*/		\
		    /* | (1<<(ALEX_PRINT-1))		*/			\
		   /*| (1<<(PORT_PRINT-1))			*/		\
		   /*| (1<<(CHECKSUM_PRINT-1))		*/  	\
			 )


#define DEBUG 1
#define DEBUG_PUTS(msg) if(DEBUG){ puts(msg); }

#define STDIN fileno(stdin)

#define BUFFER_SIZE 1024
#define TRUE 1
#define FALSE 0

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define BETWEEN(x,lo,hi) (((lo) <= (x)) && ((x) <= (hi)))
#define BETWEEN_WRAP(x,lo,hi) ((lo) > (hi) ? ((x) >= (lo) || (x) <= (hi)) : BETWEEN((x),(lo),(hi)))
#define CONGRUENT(a,b,mod) ((a) % (mod) == (b) % (mod))
#define WRAP_ADD(x,y,mod) (((x) + (y)) % (mod))

#define WRAP_DIFF(x,y,length) ((y) >= (x) ? (y) - (x) : (length) - (x) + (y)) 

#define CRASH_AND_BURN(msg) 						\
do{													\
	printf("%s: %s,%d\n", msg, __FILE__, __LINE__);	\
	exit(1);										\
}													\
while(0)

#define LOG(msg) printf msg

#define print(msg,flag)								\
do{													\
	if(((1<<((flag)-1)) & mask) > 0){							\
		printf msg ;								\
		puts("");									\
	}												\
}													\
while(0)

typedef int boolean;

/* SOME COMMON FUNCTION POINTERS */
typedef void (*destructor_f)(void**);
typedef void (*printer_f)(void*);
typedef int (*action_f)(void*);
typedef int (*comparator_f)(void*, void*);

/* USEFUL STRUCTS */
struct memchunk{
	void* data;
	int length;
};

struct buffer{
	void* data;
	int capacity;
	int length;
};

#define _pthread_mutex_lock(m)   				\
do{												\
	printf("lock %s %d", __FILE__, __LINE__);	\
	pthread_mutex_lock(m);						\
	printf("----\n");							\
}												\
while(0);

#define _pthread_mutex_unlock(m)   				\
do{												\
	printf("unlock %s %d", __FILE__, __LINE__);	\
	pthread_mutex_unlock(m);					\
	printf("----\n");							\
}												\
while(0);

typedef struct memchunk* memchunk_t;
typedef struct buffer* buffer_t;

memchunk_t memchunk_init(void* data, int length);
void memchunk_destroy(memchunk_t* chunk);
void memchunk_destroy_total(memchunk_t* chunk, destructor_f destructor);

buffer_t buffer_init(int capacity);
void buffer_destroy(buffer_t* buffer);

/// key functionality of the buffer. You try to fill it with a given
/// chunk of data, and you return how much you used of the given pointer 
/// (see chunked_queue for how to use)
int buffer_fill(buffer_t buffer, void* data, int length);

void buffer_empty(buffer_t buffer);


/* AUXILIARY METHODS */
int compare_ints(int a, int b);
void error(const char* msg);
void rtrim(char* s, const char* delim);
int utils_startswith(const char* s, const char* starts);
void util_free(void** ptr);
void inspect_bytes(const char* msg, int num_bytes);
void print_non_null_terminated(void* data, int length);
char* null_terminate(void* data, int length);
int fd_fgets(fd_set* fd, char* buffer, int size_of_buffer, FILE* file, struct timeval* tv);

#endif // __UTILS_H__
