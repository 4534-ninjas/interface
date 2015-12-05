/* This file is for Unix. It needs to be re-written for the rover. */

#include <unistd.h>
#include <err.h>

#include "queue.h"

static int fds[2];

void
queue_init(void)
{
	if (pipe(fds) == -1)
		err(1, "pipe");
}

void
queue_write(struct msg *m)
{
	if (write(fds[1], m, sizeof(*m)) != sizeof(*m))
		err(1, "write");
}

void
queue_read(struct msg *m)
{
	if (read(fds[0], m, sizeof(*m)) != sizeof(*m))
		err(1, "read");
}
