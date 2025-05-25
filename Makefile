# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
OBJ_DIR = obj
SRC_DIR = .
BIN = CSnake

# nanosleep
CFLAGS += -D_POSIX_C_SOURCE=200809L

# Find all source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Default target
all: $(BIN)

# Link the final executable
$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile each .c file to .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure obj directory exists
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Run executable
run:
	@make
	@./$(BIN)

# Clean the build
clean:
	rm -rf $(BIN) $(OBJ_DIR)

# Rebuild everything
rebuild: clean all

# Auto-generate dependencies
DEPFILES = $(OBJS:.o=.d)
-include $(DEPFILES)

# Create dependency files (.d) dynamically
$(OBJ_DIR)/%.d: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@$(CC) $(CFLAGS) -MM $< -MT $(OBJ_DIR)/$*.o -o $@

.PHONY: all clean rebuild


