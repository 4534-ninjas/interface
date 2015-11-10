CFLAGS=-Wall
example: example.o debug.o pkt.o

test_pkt: pkt.c
	$(CC) -DTEST_PKT -o $@ $<

test_find: pkt.c
	$(CC) -DTEST_FIND -o $@ $<

.PHONY: test clean
test: test_pkt test_find
	./test_pkt
	./test_find

clean:
	rm -f test_pkt test_find example *.o
