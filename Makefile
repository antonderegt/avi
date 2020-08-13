SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

EXE := $(BIN_DIR)/avi
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CPPFLAGS := -Iinclude -MMD -MP
CFLAGS   := -Wall
LDFLAGS  := -Llib
LDLIBS   := -lm

.PHONY: all clean

all: $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR)

format:
	clang-format -i src/*.c
	clang-format -i include/*.h

-include $(OBJ:.o=.d)







# CFLAGS=-c -Wall -Wextra -pedantic -std=c99
# SRCS=src/main.c src/highlight.c src/terminal.c src/find.c src/rowOperations.c src/input.c src/output.c src/history.c
# OBJS = $(SRCS:.c=.o)

# all: program

# program: $(OBJS)
# # program: src/main.o src/highlight.o src/terminal.o src/find.o src/rowOperations.o src/input.o src/output.o src/history.o
# 	$(CC) src/main.o src/highlight.o src/terminal.o src/find.o src/rowOperations.o src/input.o src/output.o src/history.o -o avi

# main.o: src/main.c
# 	$(CC) $(CFLAGS) src/main.c

# highlight.o: src/highlight.c
# 	$(CC) $(CFLAGS) src/highlight.c

# terminal.o: src/terminal.c
# 	$(CC) $(CFLAGS) src/terminal.c

# find.o: src/find.c
# 	$(CC) $(CFLAGS) src/find.c

# rowOperations.o: src/rowOperations.c
# 	$(CC) $(CFLAGS) src/rowOperations.c

# input.o: src/input.c
# 	$(CC) $(CFLAGS) src/input.c

# output.o: src/output.c
# 	$(CC) $(CFLAGS) src/output.c

# history.o: src/history.c
# 	$(CC) $(CFLAGS) src/history.c

# format:
# 	clang-format -i src/*.c

# clean:
# 	rm -rf src/*.o && rm avi
