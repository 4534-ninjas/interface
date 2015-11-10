#include <err.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "debug.h"
#include "pkt.h"

void *
do_foobar(void *x __attribute__((__unused__)))
{
	int a = 12;
	int b = 34;
	for (;;) {
		debug("thing at (%d,%d)", a, b);
		sleep(1);
	}
}

int
main()
{
	char *buf;
	size_t len;

	pthread_t foobar_td;
	pthread_create(&foobar_td, NULL, do_foobar, NULL);

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
