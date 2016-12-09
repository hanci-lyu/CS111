CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-lpthread -lrt

lab2b: SortedList.c
	@$(CC) $(CFLAGS) -o $@ $^ $@.c $(LDFLAGS)

clean:
	@rm -f lab2b *.o

dist:
	@tar -cf lab2b-004617230.tar.gz lab2b.c SortedList.c SortedList.h Makefile README first_graph.png second_graph.png
	
