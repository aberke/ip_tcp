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
	
void print_non_null_terminated(void* data, int length){
	/* print out what you got */
	char buff[length + 1];
	memcpy(buff, data, length);
	buff[length] = '\0';
	printf("data sent: %s\n", buff);
	fflush(stdout);
	/*                  	  */
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

int compare_ints(int a, int b){
	if(a<b) 	 return -1;
	else if(a>b) return 1;
	else 		 return 0;
}

void error(const char* msg){
	perror(msg);
	if (DEBUG) exit(1);
}

void inspect_bytes(const char* msg, int length){
	int i;	
	for(i=0;i<length;i++)
		printf("[%d %d %c] ", i, (int)msg[i], msg[i]);		
	
	puts("");
}

/*
 used for reading from stdin without blocking (for long periods of time).
	returns:
		0  if nothing read
		1  if something read
		-1 if error (fgets returned NULL)
*/
int fd_fgets(fd_set* set, char* buff, int size_of_buff, FILE* file_stream, struct timeval* tv){
															
	/* set up for the call */									
	FD_ZERO(set); 											
	int fd = fileno(file_stream);								
	FD_SET(fd, set);											
															
	/* get the result */										
	int ret = select(fd+1, set, NULL, NULL, tv);						
	if(ret < 0)
		return ret;

	else if(ret==0)
		return 0;
															
	if(FD_ISSET(fd, set)){
		char* ret = fgets(buff, size_of_buff, file_stream);					
		if(!ret)
			return -1;
		else
			return 1;
	}
	else{
		CRASH_AND_BURN("how did execution get here!!");
	}
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
