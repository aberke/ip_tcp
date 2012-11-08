#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <time.h>
#include <sys/select.h>

/* for printing */
#define IP_PRINT 			1
#define TCP_PRINT 			2
#define WINDOW_PRINT 		3
#define SEND_WINDOW_PRINT 	4

#define mask (  0									\
		   /*| (1<<(IP_PRINT-1))		    */		\
		   /*| (1<<(TCP_PRINT-1)) 		    */		\
		   /*| (1<<(WINDOW_PRINT-1)) 		*/		\
	   	   /*| (1<<(SEND_WINDOW_PRINT-1))	*/ 		\
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
#define WRAP_DIFF(x,y,length) ((y) >= (x) ? (y) - (x) : (length) - (x) + (y)) 
#define WRAP_ADD(x,y,mod) (((x) + (y)) % (mod))

#define CRASH_AND_BURN(msg) 						\
do{													\
	printf("%s: %s,%d\n", msg, __FILE__, __LINE__);	\
	exit(1);										\
}													\
while(0)

#define LOG(msg) printf msg

#define print(msg,flag)								\
do{													\
	if((flag & mask) > 0){							\
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
void error(const char* msg);
void rtrim(char* s, const char* delim);
int utils_startswith(const char* s, const char* starts);
void util_free(void** ptr);
void inspect_bytes(const char* msg, int num_bytes);

int fd_fgets(fd_set* fd, char* buffer, int size_of_buffer, FILE* file, struct timeval* tv);

#endif // __UTILS_H__
