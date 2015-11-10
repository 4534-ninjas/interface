#include <stdio.h>

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

	while (recv_pkt(&buf, &len) != -1) {
		printf("got packet: ");fflush(stdout);
		send_pkt(buf, len);
		printf("\n");
	}

	return 0;
}
