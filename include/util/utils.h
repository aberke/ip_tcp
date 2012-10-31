#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>

#define DEBUG 1
//#define TEST_STATES_ON
#define DEBUG_PUTS(msg) if(DEBUG){ puts(msg); }

#define STDIN fileno(stdin)

#define BUFFER_SIZE 1024
#define TRUE 1
#define FALSE 0

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define BETWEEN(x,lo,hi) (((lo) <= (x)) && ((x) < (hi)))
#define BETWEEN_WRAP(x,lo,hi) ((lo) > (hi) ? ((x) >= (lo) || (x) < (hi)) : BETWEEN((x),(lo),(hi)))
#define CONGRUENT(a,b,mod) ((a) % (mod) == (b) % (mod))
#define WRAP_DIFF(x,y,length) ((y) >= (x) ? (y) - (x) : (length) - (x) + (y)) 
#define WRAP_ADD(x,y,mod) (((x) + (y)) % (mod))

#define LOG(msg) printf msg

typedef int boolean;

/* SOME COMMON FUNCTION POINTERS */
typedef void (*destructor_f)(void**);
typedef void (*printer_f)(void*);
typedef void (*action_f)(int*);

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


#endif // __UTILS_H__
