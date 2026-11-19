UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CC ?= clang
    SHLIB_EXT = dylib
    SHLIB_FLAGS = -dynamiclib
    SHLIB_SO_TARGET = $(BIN_DIR)/libc_engine.so
else
    CC ?= gcc
    SHLIB_EXT = so
    SHLIB_FLAGS = -shared
    SHLIB_SO_TARGET =
endif

CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Wconversion -Wshadow -Wundef -Wstrict-prototypes -Iinclude -O2 -g -fPIC

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
BIN_DIR = bin

CORE_OBJS = $(BUILD_DIR)/date_utils.o \
            $(BUILD_DIR)/assignment.o \
            $(BUILD_DIR)/priority_queue.o \
            $(BUILD_DIR)/history.o \
            $(BUILD_DIR)/storage.o

TEST_ENGINE_BIN = $(BIN_DIR)/test_engine
TEST_STORAGE_BIN = $(BIN_DIR)/test_storage
CLI_BIN = $(BIN_DIR)/sapm_cli
SHLIB = $(BIN_DIR)/libc_engine.$(SHLIB_EXT)

.PHONY: all test clean shared_lib

all: $(TEST_ENGINE_BIN) $(TEST_STORAGE_BIN) $(CLI_BIN) $(SHLIB) $(SHLIB_SO_TARGET)

shared_lib: $(SHLIB) $(SHLIB_SO_TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_ENGINE_BIN): $(BUILD_DIR)/date_utils.o $(BUILD_DIR)/assignment.o $(BUILD_DIR)/priority_queue.o $(TEST_DIR)/test_engine.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(BUILD_DIR)/date_utils.o $(BUILD_DIR)/assignment.o $(BUILD_DIR)/priority_queue.o $(TEST_DIR)/test_engine.c -o $@

$(TEST_STORAGE_BIN): $(CORE_OBJS) $(TEST_DIR)/test_storage.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(CORE_OBJS) $(TEST_DIR)/test_storage.c -o $@

$(CLI_BIN): $(CORE_OBJS) $(SRC_DIR)/main.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(CORE_OBJS) $(SRC_DIR)/main.c -o $@

$(SHLIB): $(CORE_OBJS) | $(BIN_DIR)
	$(CC) $(SHLIB_FLAGS) $(CORE_OBJS) -o $@

ifeq ($(UNAME_S),Darwin)
$(BIN_DIR)/libc_engine.so: $(SHLIB) | $(BIN_DIR)
	cp -f $(SHLIB) $@
endif

test: $(TEST_ENGINE_BIN) $(TEST_STORAGE_BIN)
	@echo "\n======================================================="
	@echo "   RUNNING CORE ENGINE TEST SUITE"
	@echo "======================================================="
	@$(TEST_ENGINE_BIN)
	@echo "\n======================================================="
	@echo "   RUNNING STORAGE & PERSISTENCE TEST SUITE"
	@echo "======================================================="
	@$(TEST_STORAGE_BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) assignments_data.csv assignments_data.csv.tmp /tmp/sapm_test
