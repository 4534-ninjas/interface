#include <alloca.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void
send_pkt(char *buf, size_t len)
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
	(void)write(1, buf, p - buf);
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
extract_pkt(char *buf, size_t buf_len, char **pkt, size_t *pkt_len)
{
	char *start, *end;
	if ((start = find("\xff{", 2, buf, buf_len)) == NULL)
		return -1;
	start += 2;
	if ((end = find("\xff}", 2, start, buf_len - (start - buf))) == NULL)
		return -1;
	*pkt_len = end - start;
	if ((*pkt = malloc(*pkt_len)) == NULL)
		return -1;
	memcpy(*pkt, start, *pkt_len);
	return 0;
}

#ifdef TEST_PKT
int main() {
	char buf[] = "as\xff}df\xff{foo\xff}bar";
	char *pkt;
	size_t len;
	assert(extract_pkt(buf, strlen(buf), &pkt, &len) == 0);
	assert(pkt != NULL);
	assert(len == 3);
	assert(memcmp(pkt, "foo", len) == 0);
}
#endif

void
recv_pkt(char **buf, size_t *len)
{
	// *super* inefficient
}
