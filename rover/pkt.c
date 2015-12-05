#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pkt.h"

void
send_pkt_str(const char *str)
{
	size_t len = strlen(str);
	send_pkt(str, len);
}

void
send_pkt(const char *buf, size_t len)
{
	char *p;
	char dst[len * 2 + 4];
	size_t i;
	p = dst;
	*p++ = '\xff'; *p++ = '{';
	for (i = 0; i < len; i++) {
		switch (buf[i]) {
		case '\xff':
			*p++ = '\\';
			*p++ = 'x';
			break;
		case '\\':
			*p++ = '\\';
			*p++ = '\\';
			break;
		default:
			*p++ = buf[i];
		}
	}
	*p++ = '\xff'; *p++ = '}';
	(void)write(1, dst, p - dst); // replace with sending via uart
}
