# Makefile for compiling mt-pipeline

# Compiler and flags
CC = gcc -g
CFLAGS = -Wall -Wextra -O2 -Iinclude -pthread

# Directories
SRC_DIR = .
OBJ_DIR = .
BIN_DIR = .

# Target executable
TARGET = $(BIN_DIR)/mt-pipeline

# Source and object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Build the target
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Build object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm *.o $(TARGET)

# Phony targets
.PHONY: all clean