#include <string.h>

#include "stdlib.h"
#include "stdio.h"
#include "util/utils.h"

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
