#include <string.h>

#include "stdlib.h"
#include "stdio.h"
#include "utils.h"

/* STRUCTS */

///// MEMCHUNK
memchunk_t memchunk_init(void* data, int length){
	memchunk_t chunk = malloc(sizeof(struct memchunk));
	chunk->data = data;
	chunk->length = length;
	return chunk;
}

void memchunk_destroy(memchunk_t* chunk){
	free(*chunk);
	*chunk = NULL;
}	

void memchunk_destroy_total(memchunk_t* chunk, destructor_f destructor){
	if(destructor)
		destructor(&((*chunk)->data));
	
	free(*chunk);
	*chunk = NULL;
}	

///// BUFFER
buffer_t buffer_init(int capacity){
	buffer_t buff = malloc(sizeof(struct buffer));
	buff->data = (void*)malloc(capacity);
	buff->length = 0;
	buff->capacity = capacity;
	return buff;
}

void buffer_destroy(buffer_t* buffer){
	free(*buffer);
	*buffer = NULL;
}	

/* COPIES the data in to the buffer so the caller should take care of freeing it, 
   returns how much it was able to fill */
int buffer_fill(buffer_t buffer, void* data, int length){
	if(buffer->length + length <= buffer->capacity){
		memcpy(buffer->data + buffer->length, data, length);
		buffer->length += length;
		return length;
	}
	else{
		int remaining = buffer->capacity - buffer->length;
		memcpy(buffer->data, data, remaining);
		return length - remaining;
	}
}

void buffer_empty(buffer_t buffer){
	buffer->length = 0;
}

/* FUNCTIONS */

void error(const char* msg){
	perror(msg);
	if (DEBUG) exit(1);
}

/* takes in a string and a character and returns true
   only if the character is in the string (an empty string
   will return false */

int string_contains(const char* str, char d){
	int i;
	for(i=0;i<strlen(str);i++){	
		if( d==str[i] ) return TRUE;
	}
	return FALSE;
}

/* gets rid of all trailing characters in the string that
   are in delim */

void rtrim(char* str, const char* delim){
	int i = strlen(str);	
	while(string_contains(delim, str[--i]));	
	str[i+1] = '\0';
}

/* utils_startswith just checks if the first string starts
   with the characters in the second string. Returns 1 if 
   true, 0 otherwise */

int utils_startswith(const char* s, const char* beginning){
	if(strlen(s) < strlen(beginning))
		return 0;

	int i;
	for(i=0;i<strlen(beginning);i++)
	{	
		if(s[i] != beginning[i]) return 0;
	}

	return 1;
}	

/* useful for passing around as a function pointer */

void util_free(void** ptr){
	free(*ptr);
	*ptr = NULL;
}
