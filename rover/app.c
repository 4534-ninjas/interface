#include <assert.h> // remove
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "because_unix.h"
#include "debug.h"
#include "queue.h"
#include "uart.h"
#include "pkt.h"

void cmd_ping(const char *);
void cmd_f(const char *);
void cmd_b(const char *);
void cmd_cmds(const char *);

struct cmd {
	char *op;
	void (*callback)(const char *);
	char *descr;
	char *arg_descr; /* or NULL, when no arg expected */
} cmds[] = {
	{"CMDS", cmd_cmds, "Enumerate available commands", NULL},
	{"PING", cmd_ping, "Initiate connection heartbeat", NULL},
	{"F", cmd_f, "Move forward", "Milimeters to move"},
	{"B", cmd_b, "Move backward", "Milimeters to move"},
};

void
cmd_ping(const char *s)
{
	send_pkt_str("PONG");
}

void
cmd_f(const char *s)
{
	fprintf(stderr, "moving forward %dmm\n", atoi(s));
}

void
cmd_b(const char *s)
{
	fprintf(stderr, "moving backward %dmm\n", atoi(s));
}

void
cmd_cmds(const char *s)
{
	int i;
	for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
		char buf[1024];
		snprintf(buf, sizeof(buf), "%s\n%s%s%s",
		    cmds[i].op,
		    cmds[i].descr,
		    cmds[i].arg_descr ? "\n" : "",
		    cmds[i].arg_descr ?: "");
		send_pkt_str(buf);
	}
}

void
msg_dispatch()
{
	struct msg m;
	int i;

	queue_read(&m); // this happens to block (because unix), but you would do it non-blocking and check if we had a message or not

	// if (!had_message) return;

	// remove these
	assert(m.op[sizeof(m.op)-1] == '\0');
	assert(m.arg[sizeof(m.arg)-1] == '\0');

	for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
		if (strcasecmp(m.op, cmds[i].op) != 0)
			continue;

		// found op
		cmds[i].callback(m.arg);
		break;
	}

	// op not found
	debug("unknown op");
}

int
main()
{
	queue_init();
	uart_init();
	setup_uart_interrupt_handler();

	for (;;) {
		msg_dispatch();

		/*
		switch (state) {
		case HIDING:
			...
			break;

		case SEEKING:
			...
			break;
		...
		*/
	}
}
