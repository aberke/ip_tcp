#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util/utils.h"
#include "routing_table.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define COLUMN_WIDTH 35

#define OPTIONS "v"
#define DEFAULT_VERBOSE 0
#define DEFAULT_COMMAND 0

#define ANNOTATE(annotation)\
do{							\
	if(strlen(annotation)) printf("%s # %s %s\n", ANSI_COLOR_BLUE, annotation, ANSI_COLOR_RESET);\
	else printf("\n");		\
}							\
while(0) 

#define TEST_STR_EQ(e1,e2,annotation)										\
do{																			\
	printf("%-50s == %-20s\t\t", (#e1), (#e2));								\
	if(!strcmp(e1,e2)){														\
		printf("%sGood%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);			\
		ANNOTATE(annotation);												\
	}																		\
	else {																	\
		printf("%sBad\n", ANSI_COLOR_RED);									\
		ANNOTATE(annotation);												\
		printf("RESULTS @ %s:%d\n", __FILE__, __LINE__);					\
		printf("%s = %s\n", (#e1), e1);										\
		printf("\t\t\t%s = %s\n", (#e2), e2);								\
		printf("%s", ANSI_COLOR_RESET);										\
	}																		\
}																			\
while(0)

#define TEST_EQ(e1,e2,annotation)										\
do{																		\
	printf("%-50s == %-20s\t\t", (#e1), (#e2));							\
	if(e1==e2){															\
		printf("%sGood%s", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);			\
		ANNOTATE(annotation);											\
	}																	\
	else {																\
		printf("%sBad", ANSI_COLOR_RED);								\
		ANNOTATE(annotation);											\
		printf("RESULTS @ %s:%d\n", __FILE__, __LINE__);				\
		printf("%s = %f\n", (#e1), (float)e1);							\
		printf("\t\t\t%s = %f\n", (#e2), (float)e2);					\
		printf("%s", ANSI_COLOR_RESET);									\
	}																	\
}																		\
while(0)

#define TEST(tst)  									\
do{													\
	printf("<<	Running test: %s\t>>\n\n", (#tst));	\
	tst();											\
	puts("");										\
}													\
while(0)										


int verbose = DEFAULT_VERBOSE;

struct routing_info* fill_routing_info(int num_entries, uint32_t* costs, uint32_t* addrs){
	struct routing_info* info = malloc(sizeof(struct routing_info) + num_entries*sizeof(struct cost_address));
	info->command = DEFAULT_COMMAND;
	info->num_entries = (uint32_t)num_entries;
	
	int i;
	for(i=0;i<num_entries;i++){
		struct cost_address ca;
		ca.cost = costs[i];
		ca.address = addrs[i];
		memcpy(&info->entries[i], &ca, sizeof(struct cost_address));
	}

	return info;
}

void debug_update_routing_table(routing_table_t rt, forwarding_table_t ft, struct routing_info* info, uint32_t next_hop){
	update_routing_table(rt, ft, info, next_hop);

	if(verbose)
	{	
		puts("printing routing table...");
		routing_table_print(rt);
		puts("");
		puts("printing forwarding table...");
		forwarding_table_print(ft);
		puts("");
	}
}

//// Testing for utils
void test_util_string(){
	char* x = malloc(sizeof(char)*BUFFER_SIZE);
	strcpy(x, "Hello there someoneee");
	rtrim(x, "e"); 
	TEST_STR_EQ(x, "Hello there someon","");	
}
/*
void test_sending_routing_info(){
	routing_table_t rt = routing_table_init();
	forwarding_table_t ft = forwarding_table_init();

	int num_entries = 2;
	uint32_t costs[5] = {5,6};
	uint32_t addrs[5] = {0,1};

	uint32_t next_hop = 3;
	struct routing_info* info = fill_routing_info(num_entries, costs, addrs);

	debug_update_routing_table(rt, ft, info, next_hop);
	
	///////// TEST ///////////
	int size;
	struct routing_info* info_to_send = routing_table_RIP_response();

	
}*/

void test_unknown(){
	routing_table_t rt = routing_table_init();
	forwarding_table_t ft = forwarding_table_init();

/* FIRST INFO */
	int num_entries = 2;
	uint32_t costs[5] = {5, 6};
	uint32_t addrs[5] = {0, 1};

	uint32_t next_hop = 3;
	struct routing_info* info = fill_routing_info(num_entries, costs, addrs);

	debug_update_routing_table(rt, ft, info, next_hop);

	TEST_EQ(routing_table_get_cost(rt,0),6,"checking correctness");
	TEST_EQ(routing_table_get_cost(rt,2),-1,"checking unknown destination for forwarding table");
	TEST_EQ(routing_table_get_cost(rt,100000),-1,"checking (large) unknown destination");

	TEST_EQ(forwarding_table_get_next_hop(ft,0),3,"");
	TEST_EQ(forwarding_table_get_next_hop(ft,2),-1,"checking unknown destination for routing table");

	/* CLEAN UP */
	free(info);

	forwarding_table_destroy(&ft);
	routing_table_destroy(&rt);
}

void test_small(){
	routing_table_t rt = routing_table_init();
	forwarding_table_t ft = forwarding_table_init();

/* FIRST INFO */
	int num_entries = 2;
	uint32_t costs[5] = {5, 6};
	uint32_t addrs[5] = {0, 1};

	uint32_t next_hop = 3;
	struct routing_info* info = fill_routing_info(num_entries, costs, addrs);

/* SECOND INFO */
	int num_entries2 = 3;
	uint32_t costs2[7] = {3,9,6};
	uint32_t addrs2[7] = {0,1,2};
	
	uint32_t next_hop2 = 4;
	struct routing_info* info2 = fill_routing_info(num_entries2, costs2, addrs2);

	debug_update_routing_table(rt, ft, info, next_hop);
	debug_update_routing_table(rt, ft, info2, next_hop2);
	
// Now check that you got what you expected

	// for the routing table
	TEST_EQ(routing_table_get_cost(rt, 0),4, "checking correctness");
	TEST_EQ(routing_table_get_cost(rt, 1),7,"");
	TEST_EQ(routing_table_get_cost(rt, 2),7,"");

	// for the forwarding_table
	TEST_EQ(forwarding_table_get_next_hop(ft,2),4,"");
	TEST_EQ(forwarding_table_get_next_hop(ft,1),3,"");
	TEST_EQ(forwarding_table_get_next_hop(ft,0),4,"");
	TEST_EQ(forwarding_table_get_next_hop(ft,3),-1,"");

	/* CLEAN UP */
	free(info); free(info2);

	forwarding_table_destroy(&ft);
	routing_table_destroy(&rt);
}

void test_basic_routing(){
	routing_table_t rt = routing_table_init();
	forwarding_table_t ft = forwarding_table_init();


/* FIRST INFO */
	int num_entries = 5;
	uint32_t costs[5] = {5, 6, 19, 12, 20};
	uint32_t addrs[5] = {0, 1, 2, 3, 4};

	uint32_t next_hop = 3;
	struct routing_info* info = fill_routing_info(num_entries, costs, addrs);

/* SECOND INFO */
	int num_entries2 = 7;
	uint32_t costs2[7] = {3,9,6, 100, 9, 24, 12};
	uint32_t addrs2[7] = {0,1,2,3,4,5,6};
	
	uint32_t next_hop2 = 4;
	struct routing_info* info2 = fill_routing_info(num_entries2, costs2, addrs2);

	debug_update_routing_table(rt, ft, info, next_hop);
	debug_update_routing_table(rt, ft, info2, next_hop2);
	

// Now test that you got what you expected

	// for the routing table
	TEST_EQ(routing_table_get_cost(rt, 4),10, "");
	TEST_EQ(routing_table_get_cost(rt, 1),7, "");
	TEST_EQ(routing_table_get_cost(rt, 6),13,"");

	// for the forwarding table
	TEST_EQ(forwarding_table_get_next_hop(ft, 6),4,"");
	TEST_EQ(forwarding_table_get_next_hop(ft, 7),-1,"");
	TEST_EQ(forwarding_table_get_next_hop(ft, 4),4,"");
	TEST_EQ(forwarding_table_get_next_hop(ft, 1),3,"");
	

	/* CLEAN UP */
	forwarding_table_destroy(&ft);
	routing_table_destroy(&rt);
}

void usage(char** argv){
	printf("Usage: %s -[%s]\n", argv[0], OPTIONS);
}

void parse_arguments(int argc,char** argv){
	int i, j;
	char* arg;
	for(i=1;i<argc;++i){
		arg = argv[i];
		if(arg[0] != '-'){
			usage(argv);
			exit(0);
		}
	
		for(j=1;j<strlen(arg);j++){
			switch(arg[j]){
				case 'v': verbose = 1; break;
				default:
					printf("Unrecognized argumnet: %s\n", argv[j]);
			}
		}
	}
}
	
int main(int argc, char** argv){
	parse_arguments(argc,argv);	

	TEST(test_util_string);

	TEST(test_small);
	TEST(test_basic_routing);
	TEST(test_unknown);

	/* RETURN */
	return(0);
}
