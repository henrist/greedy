CC=gcc
CFLAGS=-I.


%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

all: greedy

greedy: greedy.o
	gcc -pthread -o $@ $^ $(CFLAGS)

clean:
	rm -rf greedy greedy.o
