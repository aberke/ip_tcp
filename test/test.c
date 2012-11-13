//#define TEST_STATES_ON

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <assert.h>

#include "utils.h"
#include "array2d.h"
#include "tcp_states.h"
#include "tcp_connection.h"
#include "routing_table.h"
#include "state_machine.h"
#include "queue.h"
#include "send_window.h"
#include "recv_window.h"
#include "ext_array.h"


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

#define ASSERT(x) assert(x)

#define TEST_OUTPUT_BAD(msg) printf("%s%s%s\n", ANSI_COLOR_RED, msg, ANSI_COLOR_RESET);

#define TEST_STR_EQ(e1,e2,annotation)										\
do{																			\
	printf("%-50s == %-20s\t\t", (#e1), (#e2));								\
	if(!strcmp(e1,e2)){														\
		printf("%sGood%s", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);			\
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

#define TEST_FALSE(cond,annotation)										\
do{																		\
	printf("!%-50s\t\t", (#cond) );										\
	if(!cond){															\
		printf("%sGood%s", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);			\
		ANNOTATE(annotation);											\
	}																	\
	else {																\
		printf("%sBad%s", ANSI_COLOR_RED, ANSI_COLOR_RESET);			\
		ANNOTATE(annotation);											\
	}																	\
}																		\
while(0)

#define TEST_TRUE(cond,annotation)										\
do{																		\
	printf("%-50s\t\t", (#cond) );										\
	if(cond){															\
		printf("%sGood%s", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);			\
		ANNOTATE(annotation);											\
	}																	\
	else {																\
		printf("%sBad%s", ANSI_COLOR_RED, ANSI_COLOR_RESET);			\
		ANNOTATE(annotation);											\
	}																	\
}																		\
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

void test_sorted_list(){
	sorted_list_t s_list = sorted_list_init((comparator_f)compare_ints);
	
	sorted_list_insert(s_list, (void*)(long)1);
	sorted_list_insert(s_list, (void*)(long)20);
	sorted_list_insert(s_list, (void*)(long)3);
	
	plain_list_t list = sorted_list_get_list(s_list);
	plain_list_el_t el;
	PLAIN_LIST_ITER(list, el)
		printf("got: %lu\n", (long)el->data);
	PLAIN_LIST_ITER_DONE(list);

	sorted_list_destroy(&(s_list));
}
	

void test_array(){
	ARRAY_DEF(int);

	struct arrayWrapper{
		ARRAY_TYPE(int) array;
	};

	struct arrayWrapper* ar = malloc(sizeof(struct arrayWrapper));
	ARRAY_INIT(ar->array, int, 10, 10, -1);

	ARRAY_PUT(ar->array, 0, 0, 100);
	ARRAY_PUT(ar->array, 0, 1, 101);
	ARRAY_PUT(ar->array, 1, 0, 102);
	ARRAY_PUT(ar->array, 2, 0, 103);
	ARRAY_PUT(ar->array, 4, 8, 556);
		
	printf("(0,0): %d, (0,1): %d, (1,0): %d\n", ARRAY_GET(ar->array, 0, 0), ARRAY_GET(ar->array, 0, 1), ARRAY_GET(ar->array, 1, 0));

	printf("(4,8): %d\n", ARRAY_GET(ar->array, 4, 8));

	ARRAY_DESTROY(&(ar->array));
}

void test_tcp_states(){
	state_machine_t machine = state_machine_init();

	tcp_connection_t connection = tcp_connection_init(NULL, 1, NULL);
	state_machine_set_argument(machine, connection);
	
	state_machine_print_state(machine);
	
	// should swith to listen
	state_machine_transition(machine, passiveOPEN);	
	state_machine_print_state(machine);
	
	// should stay in listen
	state_machine_transition(machine, activeOPEN);
	state_machine_print_state(machine);
	
	// should switch to SYN_RECVD
	state_machine_transition(machine, receiveSYN); 
	state_machine_print_state(machine);
	
	state_machine_destroy(&machine);
}

void test_send_window_scale(){
	send_window_t window = send_window_init(1.0, 100, 50, 0);
	
	char buffer[BUFFER_SIZE];
	
	int i, str_length = 10;
	strcpy(buffer, "0123456789");

	for(i=0;i<1000;i++)
		send_window_push(window, buffer, str_length);

	strcpy(buffer, "THE END");
	send_window_push(window,buffer,strlen("THE END"));

	send_window_chunk_t got;
	for(i=0;i<(1000/(50/10));i++){
		got = send_window_get_next(window);
		if(got){
			send_window_ack(window, (got->seqnum+got->length) % MAX_SEQNUM);
			send_window_chunk_destroy_total(&got, util_free);
		}
		else{
			printf(".");
		}	
	}
	puts("");

	got = send_window_get_next(window);
	
	ASSERT(got!=NULL);
	TEST_EQ(got->length, strlen("THE END"), "");
	TEST_EQ(got->seqnum, 10000, "");

	memcpy(buffer, got->data, got->length);
	buffer[got->length] = '\0';
	TEST_STR_EQ(buffer, "THE END", "");
	
	send_window_chunk_destroy_total(&got, util_free);


	send_window_destroy(&window);
}

void test_send_window(){
	send_window_t window = send_window_init(1.0, 10, 5, 0);
	
	char buffer[BUFFER_SIZE];
	
	strcpy(buffer, "Hello");
	send_window_push(window, buffer, strlen(buffer));
	
	strcpy(buffer, ", World!");
	send_window_push(window, buffer, strlen(buffer));
	
	send_window_chunk_t chunk;

	// get the first 5 bytes out of the window (should pull other stuff in)
	chunk = send_window_get_next(window);
	ASSERT(chunk!=NULL);
	TEST_EQ(chunk->length, 5, "");
	TEST_EQ(chunk->seqnum, 0, "");
	
	memcpy(buffer, chunk->data, 5);
	buffer[5] = '\0';
	TEST_STR_EQ(buffer, "Hello", "I will be amazed if this works");

	send_window_chunk_destroy_total(&chunk, util_free);


	// rinse and repeat
	chunk = send_window_get_next(window);
	ASSERT(chunk!=NULL);
	TEST_EQ(chunk->length, 5, "");
	TEST_EQ(chunk->seqnum, 5, "");
	
	memcpy(buffer, chunk->data, 5);
	buffer[5] = '\0';
	TEST_STR_EQ(buffer, ", Wor", "");

	send_window_chunk_destroy_total(&chunk, util_free);


 	// now 10 bytes should be in flight. So let's ack the first 5
	send_window_ack(window, 5);


	// and try to get the tail 
	chunk = send_window_get_next(window);
	ASSERT(chunk!=NULL);
	TEST_EQ(chunk->length, 3, "");
	TEST_EQ(chunk->seqnum, 10, "");
	
	memcpy(buffer, chunk->data, 3);
	buffer[3] = '\0';
	TEST_STR_EQ(buffer, "ld!", "");

	send_window_chunk_destroy_total(&chunk, util_free);

	send_window_ack(window, 10);
	send_window_ack(window, 13);

	// now do some more
	strcpy(buffer, "012345");
	send_window_push(window, buffer, 6);
	send_window_push(window, buffer, 6);
	send_window_push(window, buffer, 6);

	chunk = send_window_get_next(window);
	ASSERT(chunk!=NULL);
	send_window_ack(window, chunk->seqnum+chunk->length);
	send_window_chunk_destroy_total(&chunk, util_free);		

	chunk = send_window_get_next(window);
	ASSERT(chunk!=NULL);
	send_window_ack(window, chunk->seqnum+chunk->length);
	send_window_chunk_destroy_total(&chunk, util_free);		

	chunk = send_window_get_next(window);
	ASSERT(chunk!=NULL);
	send_window_ack(window, chunk->seqnum+chunk->length);
	send_window_chunk_destroy_total(&chunk, util_free);		


	send_window_destroy(&window);
}
	

void test_recv_window(){
	recv_window_t rw = recv_window_init(100, 0);

	TEST_EQ(recv_window_get_ack(rw), 1, "");
	
	memchunk_t got;

	got = recv_window_get_next(rw, 10);
	ASSERT(got==NULL);
	
	char* buffer1 = malloc(sizeof(char)*BUFFER_SIZE);
	strcpy(buffer1, "hello world");
	recv_window_receive(rw, buffer1, strlen("hello world"), 0);
	
	got = recv_window_get_next(rw, strlen("hello world"));
	ASSERT(got!=NULL);

	char buffer[BUFFER_SIZE];
	memcpy(buffer, got->data, got->length);
	buffer[got->length] = '\0';
	TEST_STR_EQ(buffer, "ello world", "");
	memchunk_destroy_total(&got, util_free);
	
	recv_window_destroy(&rw);

	/// now do it again, but with the right seqnum
	rw = recv_window_init(100, 0);

	TEST_EQ(recv_window_get_ack(rw), 1, "");
	
	got = recv_window_get_next(rw, 10);
	ASSERT(got==NULL);
	
	char* buffer2 = malloc(sizeof(char)*BUFFER_SIZE);
	strcpy(buffer2, "hello world");
	recv_window_receive(rw, buffer2, strlen("hello world"), 1);
	
	got = recv_window_get_next(rw, strlen("hello world"));
	ASSERT(got!=NULL);

	memcpy(buffer, got->data, got->length);
	buffer[got->length] = '\0';
	TEST_STR_EQ(buffer, "hello world", "");
	memchunk_destroy_total(&got, util_free);
	
	recv_window_destroy(&rw);
}

void test_recv_window_overlap(){
	recv_window_t rw = recv_window_init(100, 0);
	
	char *buffer1 = malloc(sizeof(char)*BUFFER_SIZE),
		 *buffer2 = malloc(sizeof(char)*BUFFER_SIZE),
		 *buffer3 = malloc(sizeof(char)*BUFFER_SIZE);

#define S1 "12345"
#define S2 "34567"
#define S3 "56789"

	strcpy(buffer1, S1);
	strcpy(buffer2, S2);
	strcpy(buffer3, S3);

	recv_window_receive(rw, buffer2, strlen(S1), 3);
	
	TEST_EQ(recv_window_get_ack(rw), 1, "");

	recv_window_receive(rw, buffer3, strlen(S3), 5);
	
	TEST_EQ(recv_window_get_ack(rw), 1, "");
	TEST_EQ(recv_window_get_size(rw), 100, "");
		
	recv_window_receive(rw, buffer1, strlen(S1), 1);
	
	TEST_EQ(recv_window_get_ack(rw), 10, "");
	TEST_EQ(recv_window_get_size(rw), 91, "");

	memchunk_t got = recv_window_get_next(rw, 20);
	ASSERT(got!=NULL);
	
	char buffer[256];
	memcpy(buffer, got->data, got->length);
	buffer[got->length] = '\0';
	TEST_STR_EQ(buffer, "123456789", "");
	memchunk_destroy_total(&got, util_free);
	
	recv_window_destroy(&rw);	
}

void test_wrapping(){
	/* these functions REALLY needs to be correct */
	
	int x=0,y=10,z=5;
	
	TEST_EQ(WRAP_DIFF(x,y,13),10,"");
	TEST_EQ(WRAP_DIFF(y,z,13),8, "");
	TEST_EQ(WRAP_DIFF(y,y,0), 0, "");
	TEST_EQ(WRAP_DIFF(y,x,11),1, "");
	TEST_EQ(WRAP_DIFF(z,y,12),5, "");

	TEST_EQ(WRAP_ADD(z,y,8), 7, "");
	TEST_EQ(WRAP_ADD(0,y,5), 0, "");
	TEST_EQ(WRAP_ADD(9,1,10),0, "");
	TEST_EQ(WRAP_ADD(y,101,11),1, ""); 

	TEST_FALSE(CONGRUENT(10,12,5),"");
	TEST_TRUE(CONGRUENT(10,12,2),"");
	TEST_TRUE(CONGRUENT(20,0,10),"");
	TEST_FALSE(CONGRUENT(3,0,4),"");
}

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

void test_ext_array_scale(){
	ext_array_t ar = ext_array_init(10);

	char buffer[256];
	strcpy(buffer, "Hey there you person who are something other than whatever");
	int strLength = strlen(buffer);
	
	int i, r;
	for(i=0;i<1000;i++){
		r = rand() % strLength;
		ext_array_push(ar, buffer, r);
	}
	
	memchunk_t chunk;
	for(i=0;i<1000;i++){
		r = rand() % strLength;
		chunk = ext_array_peel(ar, r);
		if(chunk)
			memchunk_destroy_total(&chunk, util_free);
	}

	ext_array_destroy(&ar);
}
		

void test_ext_array(){
	ext_array_t ar = ext_array_init(0);
	TEST_EQ_PTR(ar, NULL, "A capacity of 0 doesn't scale well");
	ext_array_destroy(&ar);

	char buffer[BUFFER_SIZE];

	ar = ext_array_init(5);
	
	char* str = malloc(sizeof(char)*(strlen("hello world")+1));
	strcpy(str, "hello world");
	
	/* this should resize it */
	ext_array_push(ar, (void*)str, strlen(str)+1);
	
	memchunk_t chunk;

	chunk = ext_array_peel(ar, 5);
	memcpy(buffer, chunk->data, 5);
	buffer[5] = '\0';
	memchunk_destroy_total(&chunk, util_free);

	TEST_STR_EQ(buffer, "hello", "");
	

	/* this should scale it down */
	chunk = ext_array_peel(ar,5);
	memcpy(buffer, chunk->data, 5);
	buffer[5] = '\0';
	memchunk_destroy_total(&chunk, util_free);

	TEST_STR_EQ(buffer, " worl", "");
	
	/* make sure this doesn't kill it */
	chunk = ext_array_peel(ar, 10);
	
	memcpy(buffer, chunk->data, chunk->length);
	buffer[chunk->length] = '\0';
	memchunk_destroy_total(&chunk, util_free);

	TEST_STR_EQ(buffer, "d", "");

	ext_array_destroy(&ar);

	free(str);
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

	TEST(test_queue); 
	
	TEST(test_ext_array);
	TEST(test_ext_array_scale);
	
	TEST(test_wrapping);
	*/
	
//	TEST(test_send_window);
//	TEST(test_send_window_scale);

	//TEST(test_recv_window);
	TEST(test_recv_window_overlap);

	//TEST(test_tcp_states);	

	//TEST(test_array);

	//TEST(test_sorted_list);

	/* RETURN */
	return(0);
}
