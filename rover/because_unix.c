#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "because_unix.h"
#include "uart.h"

// the uart_...() are all lies, but it doesn't actually matter in this case

int
uart_read_byte(void)
{
	int c = getchar();
	if (c == EOF)
		exit(0);
	return c;
}

int
uart_has_byte(void)
{
	return random() & 1;
}

static void *
on_stdin(void *x __attribute__((__unused__)))
{
	for (;;) {
		uart_interrupt_handler();
	}
	return NULL;
}

void
setup_uart_interrupt_handler(void)
{
	pthread_t td;
	pthread_create(&td, NULL, on_stdin, NULL);
}
