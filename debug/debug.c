#include <alloca.h>
#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

#include "debug.h"
#include "pkt.h"

#define PKT_DEBUG

static struct debug_entry *head = NULL;

static char *
xstrdup(const char *s)
{
	char *s2 = strdup(s);
	if (s2 == NULL)
		err(1, "strdup");
	return s2;
}

static char *
simplify(const char *fmt)
{
	const char *s;
	char *simple, *d;
	simple = d = xstrdup(fmt);
	for (s = fmt; *s != '\0'; ) {
		if (*s++ == '%') {
			if (*s == '%') {
				s++;
				continue;
			}
			if (*s == '-') s++;
			while (*s >= '0' && *s <= '9') s++;
			// XXX handle length prefixes
			switch (*s) {
			case 'd':
			case 'f':
			case 's':
				*d++ = *s++;
				break;
			default:
				*d++ = '?';
				// XXX
			}
		}
	}
	*d = '\0';
	return simple;
}

static size_t
vfmtlen(const char *simple_fmt, va_list src_ap)
{
	size_t len = 0;
	char *s;
	va_list ap;
	va_copy(ap, src_ap);
	while (*simple_fmt) {
		switch (*simple_fmt++) {
		case 'd':
			(void)va_arg(ap, int);
			len += 4;
			break;
		case 'f':
			(void)va_arg(ap, double);
			len += 8;
			break;
		case 's':
			s = va_arg(ap, char *);
			len += strlen(s) + 1;
			break;
		default:
			// XXX
			break;
		}
	}
	return len;
}

void
__debug(struct debug_entry *e, const char *file, long line, const char *fmt, ...)
{
	if (e->file == NULL) {
		/* 1st time it was called */
		e->file = file;
		e->line = line;
		e->fmt = fmt;
		e->simple_fmt = simplify(fmt);
		e->next = head;
		head = e;
	}
	e->count++;

	if (e->enabled) {
#ifdef PKT_DEBUG
		va_list ap;
		struct vals {
			uint8_t type;
			uint32_t e;
			uint32_t count;
		} __attribute__((__packed__));
		size_t buf_len = sizeof(struct vals)+vfmtlen(e->simple_fmt, ap);
		char *buf = alloca(buf_len);
		struct vals *v = (struct vals *)buf;
		v->type = 'M'; // `Message` msg type.
		v->e = htonl((uint32_t)(intptr_t)e);
		v->count = htonl((uint32_t)e->count);
		char *p = buf + sizeof(struct vals);
		int d;
		double f;
		char *s;
		size_t len;
		const char *fmtp = e->simple_fmt;
		va_start(ap, fmt);
		while (*fmt) {
			switch (*fmtp++) {
			case 'f':
				f = htonl(va_arg(ap, double));
				assert(sizeof(f) == 8);
				memcpy(p, &f, 8);
				p += 8;
			case 'd':
				d = htonl(va_arg(ap, int));
				assert(sizeof(d) == 4);
				memcpy(p, &d, 4);
				p += 4;
				break;
			case 's':
				s = va_arg(ap, char *);
				len = strlen(s) + 1;
				memcpy(p, s, len);
				p += len;
				break;
			default:
				// XXX
				break;
			}
		}
		va_end(ap);
		assert(p - buf == buf_len); // remove me?
		send_pkt(1, buf, buf_len);
#else
		(void)printf("%s:%ld(%lu) %zx ",
		    file, line, e->count, (size_t)e);
		va_list ap;
		va_start(ap, fmt);
		(void)vprintf(fmt, ap);
		va_end(ap);
#endif
	}
}

void
debug_dump(struct debug_entry *e)
{
#ifdef PKT_DEBUG
	// this is all kinda ugly :/
	struct vals {
		uint8_t type;
		uint32_t e;
		uint32_t count;
		uint32_t line;
		uint8_t enabled;
	} __attribute__((__packed__));
	size_t file_len = strlen(e->file) + 1;
	size_t fmt_len = strlen(e->fmt) + 1;
	size_t sfmt_len = strlen(e->simple_fmt) + 1;
	size_t buf_len = sizeof(struct vals) + file_len + fmt_len + sfmt_len;
	char *buf = alloca(buf_len);
	struct vals *v = (struct vals *)buf;
	v->type = 'D'; // `Definition` msg type.
	v->e = htonl((uint32_t)(intptr_t)e);
	v->count = htonl((uint32_t)e->count);
	v->line = htonl((uint32_t)e->line);
	v->enabled = (uint8_t)e->enabled;
	char *p = buf + sizeof(struct vals);
	memcpy(p, e->file, file_len); p += file_len;
	memcpy(p, e->fmt, fmt_len); p += fmt_len;
	memcpy(p, e->simple_fmt, sfmt_len); p += sfmt_len;
	assert(p - buf == buf_len); // remove me?
	send_pkt(1, buf, buf_len);
#else
	printf("%zx\t%sabled\t%lu\t%s:%ld\t%s\n",
	    (size_t)e, e->enabled ? "en" : "dis",
	    e->count, e->file, e->line, e->fmt);
#endif
}

void
debug_dump_all(void)
{
	struct debug_entry *e;
	for (e = head; e != NULL; e = e->next)
		debug_dump(e);
}

int
debug_set_enabled(uint32_t id, int enabled)
{
	struct debug_entry *e;
	// Looping to validate it's an actual debug entry.
	// Makes it O(n) instead of O(1), but is *much* safer.
	for (e = head; e != NULL; e = e->next) {
		if ((uint32_t)(intptr_t)e == id) {
			e->enabled = enabled;
			return 0; // Found.
		}
	}
	return -1; // Not found.
}
