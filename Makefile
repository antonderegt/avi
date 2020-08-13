CFLAGS=-c -Wall -Wextra -pedantic -std=c99

all: program

program: src/main.o src/highlight.o src/terminal.o src/find.o src/rowOperations.o src/input.o src/output.o src/history.o
	$(CC) src/main.o src/highlight.o src/terminal.o src/find.o src/rowOperations.o src/input.o src/output.o src/history.o -o avi

main.o: src/main.c
	$(CC) $(CFLAGS) src/main.c

highlight.o: src/highlight.c
	$(CC) $(CFLAGS) src/highlight.c

terminal.o: src/terminal.c
	$(CC) $(CFLAGS) src/terminal.c

find.o: src/find.c
	$(CC) $(CFLAGS) src/find.c

rowOperations.o: src/rowOperations.c
	$(CC) $(CFLAGS) src/rowOperations.c

input.o: src/input.c
	$(CC) $(CFLAGS) src/input.c

output.o: src/output.c
	$(CC) $(CFLAGS) src/output.c

history.o: src/history.c
	$(CC) $(CFLAGS) src/history.c

format:
	clang-format -i src/*.c

clean:
	rm -rf src/*.o && rm avi
