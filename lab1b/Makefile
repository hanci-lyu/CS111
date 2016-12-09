CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-lpthread -lmcrypt

.PHONY: server client

all: server client

server: server.c
	@$(CC) $(CFLAGS) -o $@ server.c $(LDFLAGS)

client: client.c
	@$(CC) $(CFLAGS) -o $@ client.c $(LDFLAGS)

clean:
	@rm -rf client server *.o

dist:
	@tar -cf lab1b-004617230.tar.gz server.c client.c Makefile README my.key
	
