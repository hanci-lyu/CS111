CC=gcc
CFLAGS=-Wall -g -pg
LDFLAGS=-lpthread -lrt

lab2c: SortedList.c
	@$(CC) $(CFLAGS) -o $@ $^ $@.c $(LDFLAGS)

clean:
	@rm -f lab2c *.o gmon.out

dist:
	@tar -cf lab2c-004617230.tar.gz lab2c.c SortedList.c SortedList.h Makefile README first_graph.png
	
