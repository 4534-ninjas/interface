#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "because_unix.h"
#include "debug.h"
#include "queue.h"
#include "uart.h"
#include "pkt.h"

void cmd_ping(const char *);
void cmd_f(const char *);
void cmd_b(const char *);
void cmd_cmds(const char *);
void cmd_dbgs(const char *);
void cmd_dbgt(const char *);
void cmd_dbge(const char *);
void cmd_dbgd(const char *);

struct cmd {
	char *op;
	void (*callback)(const char *);
	char *descr;
	char *arg_descr; /* or NULL, when no arg expected */
} cmds[] = {
	{"PING", cmd_ping, "Initiate connection heartbeat", NULL},
	{"F", cmd_f, "Move forward", "Milimeters to move"},
	{"B", cmd_b, "Move backward", "Milimeters to move"},
	{"CMDS", cmd_cmds, "Enumerate available commands", NULL},
	{"DBGS", cmd_dbgs, "Enumerate debug tracepoints", NULL},
	{"DBGT", cmd_dbgt, "Debug test", "Number"},
	{"DBGE", cmd_dbge, "Debug enable", "Hex Debug ID (leave out for all)"},
	{"DBGD", cmd_dbgt, "Debug disable", "Hex Debug ID (leave out for all)"},
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
cmd_dbgs(const char *s)
{
	debug_dump_all();
}


void
cmd_dbgt(const char *s)
{
	debug("test: %d", atoi(s));
}

uint32_t
str_to_addr(const char *s)
{
	// ignore error handling for now, we handle it a higher layer anyway
	uint32_t x = 0;
	const char *p;
	char c;
	for (p = s; (c = *p) != '\0'; p++) {
		x <<= 4;
		if (c >= '0' && c <= '9')
			x += c - '0';
		if (c >= 'a' && c <= 'f')
			x += c - 'a' + 10;
		if (c >= 'A' && c <= 'F')
			x += c - 'A' + 10;
	}
	return x;
}

void
handle_dbg_enable_cmd(const char *s, int enabled)
{
	if (*s == '\0') {
		debug_set_enabled_all(enabled);
	} else {
		if (debug_set_enabled(str_to_addr(s), enabled) == -1)
			send_pkt_str("bad dbg id");
	}
}

void
cmd_dbge(const char *s) // enable
{
	handle_dbg_enable_cmd(s, 1);
}

void
cmd_dbgd(const char *s) // disable
{
	handle_dbg_enable_cmd(s, 0);
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

// thread to create some dummy recurring debug values
void *
dummy_loop(void *x)
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
void
dummy_init(void)
{
	srandom(0);
	pthread_t td;
	pthread_create(&td, NULL, dummy_loop, NULL);
}

int
main()
{
	queue_init();
	uart_init();
	setup_uart_interrupt_handler();
	dummy_init();

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
