#include <string.h> /* for memset */
#include <stdio.h> /* for debugging fprintfs, remove me. */

#include "debug.h"
#include "queue.h"
#include "uart.h"

static struct msg m;
static enum {OP, ARG, FAIL} state;
static char *p;

static void
reset(void)
{
	memset(&m, 0, sizeof(m));
	state = OP;
	p = m.op;
}

// call me somewhere at the top of app.c
void
uart_init(void)
{
	reset();
}

void
uart_interrupt_handler(void)
{
	char c;
	while (uart_has_byte()) {
		c = uart_read_byte();

		switch (state) {
		case OP:
			// an argument follows
			if (c == ' ') {
				*p = '\0';
				state = ARG;
				p = m.arg;
				break;
			}

			// complete msg without args
			if (c == '\n') {
				queue_write(&m);
				reset();
				break;
			}

			// would overflow
			if (p == m.op + sizeof(m.op) - 1) {
				debug("overflow reading OP from uart: dropping pkt");
fprintf(stderr, "OP overflow: %s", m.op);
				state = FAIL;
				break;
			}

			// normal char
			*p++ = c;
			break;

		case ARG:
			// complete msg with arg
			if (c == '\n') {
				queue_write(&m);
				reset();
				break;
			}

			// would overflow
			if (p == m.arg + sizeof(m.arg) - 1) {
				debug("overflow reading ARG from uart: dropping pkt");
fprintf(stderr, "ARG overflow: %s", m.arg);
				state = FAIL;
				break;
			}

			// normal char
			*p++ = c;
			break;

		case FAIL:
			// just ignore all data until the next msg
			if (c == '\n')
				reset();
			break;
		}
	}

	// some interrupt flag might need to be cleared here
}
