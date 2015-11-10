#include <alloca.h>
#include <assert.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void *
xmalloc(size_t n)
{
	void *x = malloc(n);
	if (x == NULL)
		err(1, "malloc");
	return x;
}

static void *
xrealloc(void *p, size_t n)
{
	void *x = realloc(p, n);
	if (x == NULL)
		err(1, "realloc");
	return x;
}

void
unescape_inplace(char *buf, size_t *len)
{
	size_t si, di;
	for (si = di = 0; si < *len; si++, di++) {
		if (buf[si] == '\\') {
			if (buf[si+1] == '\\') {
				si++;
				buf[di] = '\\';
			} else if (buf[si+1] == 'x') {
				si++;
				buf[di] = '\xff';
			} else {
				// XXX illegal encoding
				// we just drop the backslash
			}
		} else {
			buf[di] = buf[si];
		}
	}
	*len = di;
}

void
send_pkt(int fd, const char *buf, size_t len)
{
	char *p, *dst;
	size_t i;
	p = dst = alloca(len*2 + 4);
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
	(void)write(fd, dst, p - dst);
}

static char *
find(char *needle, size_t n_len, char *haystack, size_t h_len)
{
	// not the best algorithm
	size_t good = 0;
	size_t i;
	for (i = 0; i < h_len; i++) {
		if (needle[good] == haystack[i]) {
			good++;
			if (good == n_len)
				return &haystack[(i+1) - good];
		} else {
			i -= good;
			good = 0;
		}
	}
	return NULL;
}

#ifdef TEST_FIND
int main() {
	assert(find("asdf", 4, "blah what wat", 13) == NULL);
	char *result = find("asdf", 4, "blah asdf wat", 13);
	assert(result != NULL);
	assert(strcmp(result, "asdf wat") == 0);
	return 0;
}
#endif

static int
extract_pkt(char *buf, size_t *buf_len, char **pkt, size_t *pkt_len)
{
	char *start, *end;
	if ((start = find("\xff{", 2, buf, *buf_len)) == NULL)
		return -1;
	start += 2;
	if ((end = find("\xff}", 2, start, *buf_len - (start - buf))) == NULL)
		return -1;
	*pkt_len = end - start;
	*pkt = xmalloc(*pkt_len);
	memcpy(*pkt, start, *pkt_len);

	// slide remaining data over
	*buf_len -= (end+2) - buf;
	memmove(buf, end+2, *buf_len);
	return 0;
}

#ifdef TEST_PKT
int main() {
	char buf[] = "as\xff}df\xff{foo\xff}barwat";
	char *pkt;
	size_t buf_len = strlen(buf), pkt_len;
	assert(extract_pkt(buf, &buf_len, &pkt, &pkt_len) == 0);
	assert(buf_len == 6);
	assert(memcmp(buf, "barwat", buf_len) == 0);
	assert(pkt_len == 3);
	assert(memcmp(pkt, "foo", pkt_len) == 0);
}
#endif

// {m,re}alloc()ing size+1 since {m,re}alloc(0) not consistant

static int
read_more(int fd, char **big_buf, size_t *big_len)
{
	char buf[1024];
	ssize_t len = read(fd, buf, sizeof(buf));
	if (len < 0)
		err(1, "read");
	if (len == 0)
		return -1;
	*big_len += len;
	*big_buf = xrealloc(*big_buf, *big_len + 1);
	memcpy(&(*big_buf)[*big_len - len], buf, len);
	return 0;
}

int
recv_pkt(int fd, char **buf, size_t *len)
{
	static char *excess;
	static size_t excess_len;

	// *super* inefficient

	if (excess == NULL) {
		excess = xmalloc(1);
		if (read_more(fd, &excess, &excess_len) == -1)
			return -1;
	}

	while (extract_pkt(excess, &excess_len, buf, len) == -1)
		if (read_more(fd, &excess, &excess_len) == -1)
			return -1;

	unescape_inplace(*buf, len);
	return 0;
}
