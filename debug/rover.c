#include <err.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "debug.h"
#include "pkt.h"

void *
loop(void *x __attribute__((__unused__)))
{
	int a = 0;
	int b = 1000;
	int c = 500;
	for (;;) {
		debug("Internal logic signals: %d, %d, %d",
			random() & 1, random() & 1, random() & 1);

		a += random() % 3 + 8;
		a %= 50;
		debug("Sensor reporting %d (mm)", a);

		b += random() % 10 - 5;
		c += random() % 10 - 5;
		debug("Encoders: left=%d right=%d (ticks)", b, c);

		sleep(1);
	}
}

int
main()
{
	char *buf;
	size_t len;

	srandom(0);

	pthread_t td;
	pthread_create(&td, NULL, loop, NULL);

	while (recv_pkt(STDIN_FILENO, &buf, &len) != -1) {
fprintf(stderr, "got %c\n", buf[0]);
		switch (buf[0]) {
		case '?':
			debug_dump_all();
			break;

		case '+':
		case '-':
			if (len != 6) {
				warnx("%c: wrong length (%zu instead of 6", buf[0], len);
				break;
			}
			debug_set_enabled(ntohl(*(uint32_t *)&buf[1]), buf[5]);
			break;

		default:
			send_pkt(STDOUT_FILENO, "E", 1);
		}

		free(buf);
	}

	return 0;
}
