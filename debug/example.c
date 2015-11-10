#include "debug.h"

void do_foobar(void) {
	int a = 12;
	int b = 34;
	debug("thing at (%d,%d)", a, b);
}

int
main()
{
	do_foobar();
	debug_dump_all();
}
