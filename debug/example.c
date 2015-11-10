#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
	debug_dump_all();

	while (recv_pkt(STDIN_FILENO, &buf, &len) != -1) {
		printf("got packet: ");fflush(stdout);
		write(STDOUT_FILENO, buf, len);
		printf("\n");

		printf("re-encoding: ");fflush(stdout);
		send_pkt(STDOUT_FILENO, buf, len);
		printf("\n");

		free(buf);
	}

	return 0;
}
