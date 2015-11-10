#include <stddef.h>

struct debug_entry {
	const char *file;
	long line;
	unsigned long count;
	const char *fmt;
	int enabled;
	struct debug_entry *next;
};

void debug_dump_all(void);
int debug_set_enabled(size_t, int);

void __debug(struct debug_entry *, const char *, long, const char *, ...);

#define debug(...) do { \
	static struct debug_entry __e; \
	__debug(&__e, __FILE__, __LINE__, __VA_ARGS__); \
} while (0)
