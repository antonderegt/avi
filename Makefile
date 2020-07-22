all: avi

avi: kilo.c
	$(CC) kilo.c -o avi -Wall -Wextra -pedantic -std=c99

clean:
	rm avi
