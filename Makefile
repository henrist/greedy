CC=gcc
CFLAGS=-I.


%.o: %.c common.h
	$(CC) -c -o $@ $< $(CFLAGS)

all: client server

client: client.o
	gcc -pthread -o $@ $^ $(CFLAGS)

server: server.o
	gcc -pthread -o $@ $^ $(CFLAGS)

clean:
	rm -rf client client.o server server.o
