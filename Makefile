EXEC_FILE=node
SRC_DIR=src
BUILD_DIR=build
INC_DIR=include

IP_DIR=ip
TCP_DIR=tcp
UTIL_DIR=util

CC=gcc
CFLAGS=-ggdb -Wall



## uthash
UTHASH_DIR=lib/uthash-1.9.6
UTHASH_INC=$(UTHASH_DIR)/src #not a mistake


_IP_OBJS=ip_node.o routing_table.o forwarding_table.o ip_utils.o link_interface.o 

_TCP_OBJS=main.o tcp_node.o tcp_utils.o tcp_node_stdin.o tcp_api.o tcp_connection.o tcp_states.o send_window.o recv_window.o #tcp_connection_state_machine_handle.o
_UTIL_OBJS=ipsum.o parselinks.o utils.o list.o bqueue.o int_queue.o queue.o ext_array.o state_machine.o ##Could use dbg.o but for now I commented out references to it in bqueue.c


IP_OBJS=$(patsubst %.o, $(IP_DIR)/%.o, $(_IP_OBJS))
TCP_OBJS=$(patsubst %.o, $(TCP_DIR)/%.o, $(_TCP_OBJS))
UTIL_OBJS=$(patsubst %.o, $(UTIL_DIR)/%.o, $(_UTIL_OBJS))

OBJS=$(patsubst %.o, $(BUILD_DIR)/%.o, $(IP_OBJS) $(TCP_OBJS) $(UTIL_OBJS))

_INCLUDE=$(INC_DIR) $(INC_DIR)/$(IP_DIR) $(INC_DIR)/$(TCP_DIR) $(INC_DIR)/$(UTIL_DIR) $(UTHASH_INC)
INCLUDE=$(patsubst %, -I%, $(_INCLUDE))

_LIBS=pthread
LIBS=$(patsubst %, -l%, $(_LIBS))

_DEPENDENT_DIRS=build build/util build/ip build/tcp test/build test/build/tcp
DEPENDENT_DIRS=$(patsubst %, directory/%, $(_DEPENDENT_DIRS))

DEFAULT_ARGS = test/ip/helper_files/A.lnx

NODE_A = test/ip/helper_files/A.lnx
NODE_B = test/ip/helper_files/B.lnx
NODE_C = test/ip/helper_files/C.lnx

#default target
default: build

#-=--=-=-=-=-=-=-=-=-=-= TESTING -=-=-=-=-=-=-=-=-=-=-=-=-==-#
TEST_EXEC_FILE=testing
TEST_DIR=test
TEST_BUILD_DIR=$(TEST_DIR)/build
TEST_DEFAULT_ARGS=
PYTEST=pyLink.py

_TEST_OBJS=test.o 
_TEST_DEP_OBJS=ip/routing_table.o ip/forwarding_table.o util/utils.o util/state_machine.o util/queue.o tcp/send_window.o util/ext_array.o tcp/recv_window.o tcp/tcp_connection.o tcp/tcp_states.o util/bqueue.o tcp/tcp_utils.o ip/ip_utils.o util/ipsum.o ip/ip_node.o ip/link_interface.o util/parselinks.o util/list.o #tcp/tcp_connection_state_machine_handle.o
TEST_OBJS=$(patsubst %.o, $(TEST_BUILD_DIR)/%.o, $(_TEST_OBJS)) $(patsubst %.o, $(BUILD_DIR)/%.o, $(_TEST_DEP_OBJS))

_TEST_INCLUDE=$(TEST_DIR)/include
TEST_INCLUDE=$(patsubst %, -I%, $(_TEST_INCLUDE)) $(INCLUDE)

test_link:
	$(CC) $(CFLAGS) -o $(TEST_EXEC_FILE) $(TEST_OBJS)

test_build: validate $(TEST_OBJS) test_link

$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) $(TEST_INCLUDE) -c $< -o $@

test_clean:
	rm -f $(TEST_EXEC_FILE)
	rm -f $(TEST_BUILD_DIR)/*.o

test_rebuild: test_clean test_build

test_valgrind: test_rebuild
	valgrind --leak-check=full ./$(TEST_EXEC_FILE) $(TEST_DEFAULT_ARGS)

test: test_rebuild
	@./$(TEST_EXEC_FILE) $(TEST_DEFAULT_ARGS)
#-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-#


# MAIN TARGETS
link: 
	$(CC) $(CFLAGS) $(INCLUDE) $(LIB_DIRS) $(LIBS) -o $(EXEC_FILE) $(OBJS)

build: validate $(OBJS) link

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

clean: 
	rm -f `find $(BUILD_DIR) | grep .o`
	rm -f $(EXEC_FILE)

validate: $(DEPENDENT_DIRS)

directory/%: 	
	@[ -d $(patsubst directory/%, %, $@) ] || (echo "Creating directory $(patsubst directory/%, %, $@)..." && mkdir $(patsubst directory/%, %, $@))

rebuild: clean build

#run: rebuild
run:
	@./$(EXEC_FILE) $(DEFAULT_ARGS)

#runA: rebuild
runA:
	@./$(EXEC_FILE) $(NODE_A)
	
#runB: rebuild
runB:
	@./$(EXEC_FILE) $(NODE_B)
	
runC: rebuild
	@./$(EXEC_FILE) $(NODE_C)

runValgrind:
	valgrind --leak-check=full ./$(EXEC_FILE) $(NODE_A)
	
runNetwork: rebuild
	cp node test/ip/helper_files/
	cd test/ip/helper_files; ./runNetwork ABC.net


