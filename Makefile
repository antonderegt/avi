CFLAGS= -Wall -Wextra -pedantic -std=c99

all: program

program: src/main.o
	$(CC) src/main.o -o avi

main.o: src/main.c
	$(CC) $(CFLAGS) src/main.c

format:
	clang-format -i src/*.c

clean:
	rm -rf src/*.o && rm avi
