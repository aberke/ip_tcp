#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>

#define DEBUG 1
#define DEBUG_PUTS(msg) if(DEBUG){ puts(msg); }

#define STDIN fileno(stdin)

#define BUFFER_SIZE 1024
#define TRUE 1
#define FALSE 0

#define LOG(msg) printf msg

typedef int boolean;

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

/* SOME COMMON FUNCTION POINTERS */
typedef void (*destructor_f)(void**);
typedef void (*printer_f)(void*);

#endif // __UTILS_H__
