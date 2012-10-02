#include "stdio.h"
#include "util/utils.h"

void error(char* msg){
	perror(msg);
	if (DEBUG) exit(1);
}
