CC=gcc
CFLAGS=-Wall -g
LDFLAGS=

lab3a:
	@$(CC) $(CFLAGS) -o $@ $^ $@.c $(LDFLAGS)

clean:
	@rm -f lab3a *.o

dist:
	@tar -cf lab3a-004617230.tar.gz lab3a.c Makefile README	
