/* See LICENSE file for copyright and license details. */
#include <stdint.h>

extern char buf[1024];

#define LEN(x) (sizeof(x) / sizeof((x)[0]))

extern char *argv0;

__attribute__((__cold__))
void warn(const char *, ...);
__attribute__((__noreturn__))
__attribute__((__cold__))
void die(const char *, ...);

int esnprintf(char *str, size_t size, const char *fmt, ...);
const char *bprintf(const char *fmt, ...);
const char *fmt_human(uintmax_t num, int base);
int pscanf(const char *path, const char *fmt, ...);
