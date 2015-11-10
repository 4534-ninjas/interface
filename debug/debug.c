#include <stdarg.h>
#include <stdio.h>

#include "debug.h"

static struct debug_entry *head = NULL;

void
__debug(struct debug_entry *e, const char *file, long line, const char *fmt, ...)
{
	if (e->file == NULL) {
		/* 1st time it was called */
		e->file = file;
		e->line = line;
		e->fmt = fmt;
		e->next = head;
		head = e;
	}
	e->count++;

	if (e->enabled) {
		(void)printf("%s:%ld(%lu) %zx ",
		    file, line, e->count, (size_t)e);
		va_list ap;
		va_start(ap, fmt);
		(void)vprintf(fmt, ap);
		va_end(ap);
	}
}

void
debug_dump(struct debug_entry *e)
{
	printf("%zx\t%sabled\t%lu\t%s:%ld\t%s\n",
	    (size_t)e, e->enabled ? "en" : "dis",
	    e->count, e->file, e->line, e->fmt);
}

void
debug_dump_all(void)
{
	struct debug_entry *e;
	for (e = head; e != NULL; e = e->next)
		debug_dump(e);
}

int
debug_set_enabled(size_t id, int enabled)
{
	struct debug_entry *e;
	// Looping to validate it's an actual debug entry.
	// Makes it O(n) instead of O(1), but is *much* safer.
	for (e = head; e != NULL; e = e->next) {
		if ((size_t)e == id) {
			e->enabled = enabled;
			return 0; // Found.
		}
	}
	return -1; // Not found.
}
