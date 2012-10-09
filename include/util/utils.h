#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>

#define DEBUG 1

#define BUFFER_SIZE 1024
#define TRUE 1
#define FALSE 0

void error(const char* msg);
void rtrim(char* s, const char* delim);
int utils_startswith(const char* s, const char* starts);
#endif // __UTILS_H__
