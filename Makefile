all: format avi

avi: kilo.c
	$(CC) kilo.c -o avi -Wall -Wextra -pedantic -std=c99

format:
	clang-format -i *.c

clean:
	rm avi
