CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_XOPEN_SOURCE=700
TARGET_EXEC ?= cassini

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src
IDIR ?= ./include
RUN_DIR ?= ./run

CFLAGS += -I $(IDIR)

SRCS := $(shell find $(SRC_DIRS) -name *.c)
OBJS ?= ./build/./src/timing-text-io.c.o ./build/./src/common.c.o

all:cassini saturnd

cassini:./build/./src/cassini.c.o ./build/./src/timing-text-io.c.o ./build/./src/common.c.o ./build/./src/tasks.c.o
	$(CC) $(CFLAGS) $^ -o $@

saturnd:./build/./src/saturnd.c.o ./build/./src/timing-text-io.c.o ./build/./src/common.c.o ./build/./src/tasks.c.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC)  $(CFLAGS) -c $< -o $@


.PHONY: distclean

distclean:
	$(RM) -r $(BUILD_DIR)
	$(RM) $(TARGET_EXEC)
	$(RM) saturnd
	$(RM) -r $(RUN_DIR)
	$(RM) -r ./runs
	$(RM) logs

test:
	bash run-cassini-tests.sh


MKDIR_P ?= mkdir -p
