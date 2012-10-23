#include <string.h>

#include "stdlib.h"
#include "stdio.h"
#include "utils.h"

/* STRUCTS */

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
