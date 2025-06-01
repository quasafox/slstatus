#ifndef UTIL_AUX_H_
#define UTIL_AUX_H_

#include <stdbool.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(__has_builtin) && __has_builtin(__builtin_unreachable)
#  define ALWAYS_UNREACHABLE() __builtin_unreachable()
#else
#  define ALWAYS_UNREACHABLE() do {} while (0)
#endif

#if defined(__has_builtin) && __has_builtin(__builtin_expect)
#  define likely(x) __builtin_expect(!!(x), true)
#else
#  define likely(x) (x)
#endif

#if defined(__has_builtin) && __has_builtin(__builtin_expect)
#  define unlikely(x) __builtin_expect(!!(x), false)
#else
#  define unlikely(x) (x)
#endif

#if defined(__has_attribute) && __has_attribute(__const__)
#  define __const __attribute__((__const__))
#else
#  define __const
#endif

static inline bool
read_whole_file(const char *path, void *buf, size_t *bufsize)
{
	int fd;
	ssize_t nread;

	if (unlikely((fd = open(path, O_RDONLY)) < 0))
		return false;

	if (unlikely((nread = read(fd, buf, *bufsize)) < 0)) {
		close(fd);
		return false;
	}

	close(fd);

	*bufsize = (size_t)nread;

	return true;
}

struct line_iter {
	char *str;
	size_t len;
};

#define LINE_ITER(s, l) ((struct line_iter) { \
	.str = (s), \
	.len = (l) \
})

static inline bool
line_iter_next(struct line_iter *restrict iter, char **restrict out, size_t *outlen)
{
	char *newline;
	size_t len;

	if (!iter->len || !(newline = memchr(iter->str, '\n', iter->len)))
		return false;

	len = newline - iter->str;
	*outlen = len++;
	*out = iter->str;
	iter->str += len;
	iter->len -= len;

	return true;
}

struct split_string {
	char *x, *y;
	size_t xlen, ylen;
};

static inline bool
split_string(char *str, size_t len, char on, struct split_string *out)
{
	char *ptr;

	if (unlikely(!(ptr = memchr(str, on, len))))
		return false;

	out->x = str;
	out->xlen = ptr - str;
	out->y = ++ptr;
	out->ylen = len - (size_t)(ptr - str);

	return true;
}

static inline char *
trim_string_left(char *str, size_t *len)
{
	size_t offs = 0;

	while (likely(offs != *len) && (str[offs] == ' ' || str[offs] == '\t'))
		++offs;

	*len -= offs;
	return &str[offs];
}

__const static inline uint64_t
hash_bytes(const char *bytes, size_t len)
{
	uint64_t hash = 0xcbf29ce484222325;

	for (size_t i = 0; i != len; ++i) {
		hash *= 0x100000001b3;
		hash ^= bytes[i];
	}

	return hash;
}

static inline size_t
parse_size_t(const char *str)
{
	return (size_t)strtoull(str, NULL, 10);
}

#endif // !UTIL_AUX_H_
