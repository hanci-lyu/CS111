CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-lpthread -lrt

lab2a:
	@$(CC) $(CFLAGS) -o $@ lab2a.c $(LDFLAGS)

clean:
	@rm -rf lab2a *.o

dist:
	@tar -cf lab2a-004617230.tar.gz lab2a.c Makefile README time_vs_iteration.png time_vs_threads.png
	
