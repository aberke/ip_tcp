#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "utils.h"
#include "routing_table.h"
#include "state_machine.h"
#include "config.h"
#include "queue.h"
#include "window.h"

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

#define TEST_OUTPUT_BAD(msg) printf("%s%s%s\n", ANSI_COLOR_RED, msg, ANSI_COLOR_RESET);

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

#define TEST_EQ_PTR(e1,e2,annotation)									\
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
		printf("\t\t\t%s = %p\n", (#e1), e1);							\
		printf("\t\t\t%s = %p\n", (#e2), e2);							\
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

///////////////////////////// AUXILIARY FUNCTIONS ///////////////////////////////////
struct routing_info* fill_routing_info(int num_entries, uint32_t* costs, uint32_t* addrs){
	struct routing_info* info = malloc(sizeof(struct routing_info) + num_entries*sizeof(struct cost_address));
	info->command = ntohs(DEFAULT_COMMAND);
	info->num_entries = ntohs((uint32_t)num_entries);
	
	int i;
	for(i=0;i<num_entries;i++){
		struct cost_address ca;
		ca.cost = ntohs(costs[i]);
		ca.address = addrs[i];
		memcpy(&info->entries[i], &ca, sizeof(struct cost_address));
	}

	return info;
}

void debug_update_routing_table(routing_table_t rt, forwarding_table_t ft, struct routing_info* info, uint32_t next_hop){
	update_routing_table(rt, ft, info, next_hop,EXTERNAL_INFORMATION);

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
/////////////////////////////////////////////////////////////////////////////////////////

void test_queue(){
	queue_t q = queue_init();
	
	int a,b,c;
	a = 0; b = 1; c = 2;

	queue_push(q, &a);
	queue_push(q, &b);
	
	TEST_EQ(*(int*)queue_pop(q), a, "should be FIFO");
	
	queue_push(q, &c);
	
	TEST_EQ(*(int*)queue_pop(q), b, "");
	TEST_EQ(*(int*)queue_pop(q), c, "");
	TEST_EQ_PTR(queue_pop(q), NULL, "");
	
	queue_destroy(&q);
}
	
/* test that destroying windows works properly */
void test_window_destroy(){
	window_t window = window_init(1.0,1,NULL);
	
	int a=1, b=2;
	memchunk_t mem_a = memchunk_init(&a, sizeof(int)); 

	window_push(window, mem_a);
	
	window_chunk_t wc = window_get_next(window);
	window_chunk_destroy(&wc);

	window_destroy(&window);
}

void test_window(){
	window_t window = window_init(1.0, 1, NULL);
	
	int a=1, b=2, c=3;
	memchunk_t mem_a = memchunk_init(&a, sizeof(int)), 
				mem_b = memchunk_init(&b, sizeof(int)),
				mem_c = memchunk_init(&c, sizeof(int));

	window_push(window, mem_a);
	window_push(window, mem_b);
	window_push(window, mem_c);

	window_chunk_t chunk;

	chunk = window_get_next(window);
	TEST_EQ_PTR(chunk->chunk, mem_a, "FIFO window");
	TEST_EQ(chunk->seqnum, 0, "");
	window_chunk_destroy(&chunk);
	
	chunk = window_get_next(window);
	TEST_EQ_PTR(chunk, NULL, "Nothing to send");

	window_ack(window, 0);
	
	chunk = window_get_next(window);
	TEST_EQ_PTR(chunk->chunk, mem_b, "After acking, there's now something to send");
	TEST_EQ(chunk->seqnum, 0, "");
	window_chunk_destroy(&chunk);

	window_destroy(&window);
}


//// Testing the state machine
void test_state_machine(){
	state_machine_t machine = state_machine_init();

	// current state
	print_state( state_machine_get_state(machine) );

	// transition with T1, should --> S2
	state_machine_transition(machine, T1);
	print_state( state_machine_get_state(machine) );

	// transition with T2, should --> S3
	state_machine_transition(machine, T1);
	print_state( state_machine_get_state(machine) );

	// nothing should change anymore
	state_machine_transition(machine, T1);
	print_state( state_machine_get_state(machine) );
	
	// ditto
	state_machine_transition(machine, T2);	
	print_state( state_machine_get_state(machine) );

	state_machine_destroy(&machine);	
}

//// Testing for utils
void test_util_string(){
	char* x = malloc(sizeof(char)*BUFFER_SIZE);
	strcpy(x, "Hello there someoneee");
	rtrim(x, "e"); 
	TEST_STR_EQ(x, "Hello there someon","");	
}

void test_routing_info(){
	routing_table_t rt = routing_table_init();
	forwarding_table_t ft = forwarding_table_init();

	//// init the information
	int num_entries = 2;
	uint32_t costs[2] = {5,6};
	uint32_t addrs[2] = {0,1};

	uint32_t next_hop = 3;
	struct routing_info* info = fill_routing_info(num_entries, costs, addrs);

	int num_entries2 = 1;
	uint32_t costs2[1] = {4};
	uint32_t addrs2[1] = {0};

	uint32_t next_hop2 = 4;
	struct routing_info* info2 = fill_routing_info(num_entries2, costs2, addrs2); 

	//// fill the routing table
	debug_update_routing_table(rt, ft, info, next_hop);
	debug_update_routing_table(rt, ft, info2, next_hop2);
	
	///////// TEST ///////////
	int size;
	struct routing_info* info_to_send = routing_table_RIP_response(rt,next_hop,&size,EXTERNAL_INFORMATION);
	struct cost_address *cost_info1, *cost_info2;	

	TEST_EQ(info_to_send->num_entries,ntohs(2),"Checking that there are the right number of entries");

	cost_info1 = &info_to_send->entries[0];
	cost_info2 = &info_to_send->entries[1];

	if(cost_info1->address == 1){
		TEST_EQ(cost_info2->cost,htons(5),"checking that we're not poisoning good values");
		TEST_EQ(cost_info1->cost,htons(INFINITY),"checking that poison reverse is working");
	}
	else if(cost_info2->address == 1){
		TEST_EQ(cost_info1->cost,htons(5),"checking taht we're not poisoning good values");
		TEST_EQ(cost_info2->cost,htons(INFINITY),"checking that poison reverse is working");
	}
	else{
		TEST_OUTPUT_BAD("Unable to find info for address 1");
	}
	
	/* CLEANUP */
	free(info_to_send);
	routing_table_destroy(&rt);
	forwarding_table_destroy(&ft);
}

void test_overflow(){
	routing_table_t rt = routing_table_init();
	forwarding_table_t ft = forwarding_table_init();

	int num_entries = 2;
	uint32_t costs[1] = {8};
	uint32_t addrs[1] = {10000};

	uint32_t next_hop = 3;
	struct routing_info* info = fill_routing_info(num_entries, costs, addrs);

	debug_update_routing_table(rt, ft, info, next_hop);

	TEST_EQ(forwarding_table_get_next_hop(ft, 0), -1, "empty forwarding table");
	TEST_EQ(forwarding_table_get_next_hop(ft, 1000034), -1, "");

	forwarding_table_print(ft);
	routing_table_print(rt);
	
	free(info);
	routing_table_destroy(&rt);
	forwarding_table_destroy(&ft);
}

void test_empty(){
	routing_table_t rt = routing_table_init();
	forwarding_table_t ft = forwarding_table_init();

	TEST_EQ(forwarding_table_get_next_hop(ft, 0), -1, "empty forwarding table");
	TEST_EQ(forwarding_table_get_next_hop(ft, 10000), -1, "");

	routing_table_destroy(&rt);
	forwarding_table_destroy(&ft);
}

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

	TEST_EQ(routing_table_get_cost(rt,0),ntohs(6),"checking correctness");
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
	TEST_EQ(routing_table_get_cost(rt, 0),htons(4), "checking correctness");
	TEST_EQ(routing_table_get_cost(rt, 1),htons(7),"");
	TEST_EQ(routing_table_get_cost(rt, 2),htons(7),"");

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

void test_basic_routing_2(){
	routing_table_t rt = routing_table_init();
	forwarding_table_t ft = forwarding_table_init();


/* FIRST INFO */
	int num_entries = 1;
	uint32_t costs[1] = {5};
	uint32_t addrs[1] = {0};

	uint32_t next_hop = 3;
	struct routing_info* info = fill_routing_info(num_entries, costs, addrs);

/* SECOND INFO */
	int num_entries2 = 1;
	uint32_t costs2[1] = {3};
	uint32_t addrs2[1] = {0};
	
	uint32_t next_hop2 = 4;
	struct routing_info* info2 = fill_routing_info(num_entries2, costs2, addrs2);

	debug_update_routing_table(rt, ft, info, next_hop);
	debug_update_routing_table(rt, ft, info2, next_hop2);
	

// Now test that you got what you expected

	// for the routing table
	TEST_EQ(routing_table_get_cost(rt, 0), htons(4), "");

	TEST_EQ(forwarding_table_get_next_hop(ft, 0), 4, "");
	

	/* CLEAN UP */
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

	uint32_t next_hop = 11;
	struct routing_info* info = fill_routing_info(num_entries, costs, addrs);

/* SECOND INFO */
	int num_entries2 = 7;
	uint32_t costs2[7] = {3,9,6, 100, 9, 24, 12};
	uint32_t addrs2[7] = {0,1,2,3,4,5,6};
	
	uint32_t next_hop2 = 12;
	struct routing_info* info2 = fill_routing_info(num_entries2, costs2, addrs2);

	debug_update_routing_table(rt, ft, info, next_hop);
	debug_update_routing_table(rt, ft, info2, next_hop2);
	

// Now test that you got what you expected

	// for the routing table
	TEST_EQ(routing_table_get_cost(rt, 4),htons(10), "");
	TEST_EQ(routing_table_get_cost(rt, 1),htons(7), "");
	TEST_EQ(routing_table_get_cost(rt, 6),htons(13),"");

	// for the forwarding table
	TEST_EQ(forwarding_table_get_next_hop(ft, 6),12,"");
	TEST_EQ(forwarding_table_get_next_hop(ft, 7),-1,"");
	TEST_EQ(forwarding_table_get_next_hop(ft, 4),12,"");
	TEST_EQ(forwarding_table_get_next_hop(ft, 1),11,"");
	

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

	/*TEST(test_util_string);
	TEST(test_routing_info);
	TEST(test_small);
	TEST(test_basic_routing);
	TEST(test_basic_routing_2);
	TEST(test_unknown);

	TEST(test_empty);
	TEST(test_overflow);

	TEST(test_state_machine); 

	TEST(test_queue); */
	TEST(test_window);
	TEST(test_window_destroy);

	/* RETURN */
	return(0);
}
