#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "debug.h"
#include "pkt.h"

void do_foobar(void) {
	int a = 12;
	int b = 34;
	debug("thing at (%d,%d)", a, b);
}

int
main()
{
	char *buf;
	size_t len;

	do_foobar();

	while (recv_pkt(STDIN_FILENO, &buf, &len) != -1) {
		switch (buf[0]) {
		case '?':
			debug_dump_all();
			break;

		case '+':
		case '-':
			assert(len == 6);
			debug_set_enabled(ntohl(*(uint32_t *)&buf[1]), buf[5]);
			break;

		case 'f':
			do_foobar();
			break;

		default:
			send_pkt(STDOUT_FILENO, "E", 1);
		}

		free(buf);
	}

	return 0;
}
