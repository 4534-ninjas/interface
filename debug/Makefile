CFLAGS=-Wall
LDFLAGS=-pthread

.PHONY: all test clean

all: rover example

rover: rover.o debug.o pkt.o

example: example.o debug.o pkt.o

test_pkt: pkt.c
	$(CC) -DTEST_PKT -o $@ $<

test_find: pkt.c
	$(CC) -DTEST_FIND -o $@ $<

test: test_pkt test_find
	./test_pkt
	./test_find

clean:
	rm -f test_pkt test_find example rover core *.o
