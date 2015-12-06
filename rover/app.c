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
void cmd_tsts(const char *);
void cmd_tstr(const char *);

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
	{"DBGD", cmd_dbgd, "Debug disable", "Hex Debug ID (leave out for all)"},
	{"TSTS", cmd_tsts, "Enumerate tests", NULL},
	{"TSTR", cmd_tstr, "Run tests", "Test name"},
};

enum { TEST_SUCCESS, TEST_FAIL, TEST_IDK, N_STATUSES };

int test_success(void)	{ return TEST_SUCCESS; }
int test_fail(void)	{ return TEST_FAIL; }
int test_complete(void)	{ return TEST_IDK; }
int test_rand(void)	{ return random() % N_STATUSES; }

struct test {
	int (*run)(void);
	const char *name;
	const char *descr;
} tests[] = {
	{test_success, "yes", "Always passes"},
	{test_fail, "no", "Always fails"},
	{test_complete, "maybe", "Just completes"},
	{test_rand, "random", "Random result"},
};

void
cmd_tsts(const char *s)
{
	int i;
	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		char buf[100];
		snprintf(buf, sizeof(buf), "T%s\n%s",
		    tests[i].name, tests[i].descr);
		send_pkt_str(buf);
	}
}

void
cmd_tstr(const char *s)
{
	int i;
	char buf[100];
	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		if (strcasecmp(tests[i].name, s) == 0) {
			snprintf(buf, sizeof(buf), "Stesting %s", tests[i].name);
			send_pkt_str(buf);
			sleep(1);
			switch (tests[i].run()) {
			case TEST_SUCCESS:
				snprintf(buf, sizeof(buf), "R%s %s",
				    tests[i].name, "success");
				break;

			case TEST_FAIL:
				snprintf(buf, sizeof(buf), "R%s %s",
				    tests[i].name, "fail");
				break;

			case TEST_IDK:
				snprintf(buf, sizeof(buf), "R%s %s",
				    tests[i].name, "complete");
				break;
			}
			send_pkt_str(buf);
			return;
		}
	}
}

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
		snprintf(buf, sizeof(buf), "C%s\n%s%s%s",
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
		uint32_t addr = str_to_addr(s);
		if (debug_set_enabled(addr, enabled) == -1)
			debug("bad dbg id: %u", addr);
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
fprintf(stderr, "calling %s(%s)\n", cmds[i].op, m.arg);
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

	send_pkt_str("Sboot");

	cmd_cmds("");
	cmd_tsts("");
	cmd_dbgs("");

	for (;;) {
		send_pkt_str("SListen");
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
