CFLAGS=-Wall -Wextra -std=c99 -pedantic -ggdb
#LDFLAGS=-s
CC=clang

n2pk: main.o
	$(CC) $(LDFLAGS) -o $@ $^

main.o: main.c

.PHONY: clean

clean:
	rm -f  *.o n2pk
