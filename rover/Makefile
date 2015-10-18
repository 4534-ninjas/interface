CFLAGS= -Wall -g
LDFLAGS= -pthread

app: app.o because_unix.o debug.o pkt.o queue.o uart.o

clean:
	rm -f *.o app

run: app
	../debug/ncexec.py 127.0.0.1 4321 ./app
