EXEC_FILE=node
SRC_DIR=src
BUILD_DIR=build
INC_DIR=include
DEFAULT_ARGS=

## uthash
UTHASH_DIR=lib/uthash-1.9.6
UTHASH_INC=$(UTHASH_DIR)/src #not a mistake

CC=gcc
CFLAGS=-g -Wall

_OBJS=main.o routing_table.o forwarding_table.o link_interface.o
OBJS=$(patsubst %.o, $(BUILD_DIR)/%.o, $(_OBJS))

_INCLUDE=$(INC_DIR) $(UTHASH_INC)
INCLUDE=$(patsubst %, -I%, $(_INCLUDE))

# TESTING
TEST_EXEC_FILE=testing
TEST_DIR=test
TEST_BUILD_DIR=$(TEST_DIR)/build
TEST_DEFAULT_ARGS=

_TEST_OBJS=test.o
_TEST_DEP_OBJS=routing_table.o forwarding_table.o
TEST_OBJS=$(patsubst %.o, $(TEST_BUILD_DIR)/%.o, $(_TEST_OBJS)) $(patsubst %.o, $(BUILD_DIR)/%.o, $(_TEST_DEP_OBJS))

test_build: $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $(TEST_EXEC_FILE) $(TEST_OBJS)
	@echo "Tests compiled..."

$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

test_clean:
	@echo "------------------Cleaning------------------"
	rm -f $(TEST_EXEC_FILE)
	rm -f $(TEST_BUILD_DIR)/*.o
	@echo ""

echo_compile:
	@echo "******************Compiling*****************"

test_rebuild: test_clean echo_compile test_build

test: test_rebuild
	@echo ""
	@echo "==================Testing==================="
	@./$(TEST_EXEC_FILE) $(TEST_DEFAULT_ARGS)
	@echo ""

# MAIN TARGETS
link: 
	$(CC) $(CFLAGS) $(INCLUDE) $(LIB_DIRS) $(LIBS) -o $(EXEC_FILE) $(OBJS)

build: $(OBJS) link


$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "compiling $<"
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

clean: 
	rm -f $(BUILD_DIR)/*.o
	rm -f $(EXEC_FILE)

rebuild: clean build

run: 
	@./$(EXEC_FILE) $(DEFAULT_ARGS)

