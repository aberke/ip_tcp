#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>

#define DEBUG 1

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

typedef struct memchunk* memchunk_t;

memchunk_t memchunk_init(void* data, int length);
void memchunk_destroy(memchunk_t* chunk);

/* AUXILIARY METHODS */
void error(const char* msg);
void rtrim(char* s, const char* delim);
int utils_startswith(const char* s, const char* starts);

/* SOME COMMON FUNCTION POINTERS */
typedef void (*destructor_f)(void**);
typedef void (*printer_f)(void*);

#endif // __UTILS_H__
