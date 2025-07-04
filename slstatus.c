/* See LICENSE file for copyright and license details. */
#include <sys/stat.h>
#include <sys/uio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include "arg.h"
#include "slstatus.h"
#include "util.h"
#include "util-aux.h"

struct arg {
	const char *(*func)(const char *);
	const char *fmt;
	const char *args;
};

char buf[1024];
static volatile sig_atomic_t done;

#include "config.h"

static void
terminate(const int signo)
{
	if (signo != SIGUSR1)
		done = 1;
}

static void
difftimespec(struct timespec *res, struct timespec *a, struct timespec *b)
{
	res->tv_sec = a->tv_sec - b->tv_sec - (a->tv_nsec < b->tv_nsec);
	res->tv_nsec = a->tv_nsec - b->tv_nsec +
	               (a->tv_nsec < b->tv_nsec) * 1E9;
}

static void
usage(void)
{
	die("usage: %s [-v] [-s] [-1]", argv0);
}

int
main(int argc, char *argv[])
{
	struct sigaction act;
	struct timespec start, current, diff, intspec, wait;
	struct stat statb;
	size_t i, len;
	int ret;
	char status[MAXLEN+1];
	const char *res;
	int stdout_pipe;

	ARGBEGIN {
	case 'v':
		die("slstatus-"VERSION);
	case '1':
		done = 1;
		/* FALLTHROUGH */
	default:
		usage();
	} ARGEND

	if (argc)
		usage();

	memset(&act, 0, sizeof(act));
	act.sa_handler = terminate;
	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	act.sa_flags |= SA_RESTART;
	sigaction(SIGUSR1, &act, NULL);

	if (fstat(STDOUT_FILENO, &statb) < 0)
		die("failed to fstat() stdout:");
	stdout_pipe = S_ISFIFO(statb.st_mode);

	do {
		if (unlikely(clock_gettime(CLOCK_MONOTONIC, &start) < 0))
			die("clock_gettime:");

		status[0] = '\0';
		for (i = len = 0; i < LEN(args); i++) {
			if (!(res = args[i].func(args[i].args)))
				res = unknown_str;

			if (unlikely((ret = esnprintf(status + len, sizeof(status) - len,
			                              args[i].fmt, res)) < 0))
				break;

			len += ret;
		}
		status[len++] = '\n';

		if (stdout_pipe) {
			const struct iovec iobuf = {
			    .iov_base = status,
			    .iov_len = len
			};

			if (unlikely(vmsplice(STDOUT_FILENO, &iobuf, 1, 0) < 0))
				warn("vmsplice():");
		} else {
			if (unlikely(write(STDOUT_FILENO, status, len) < 0))
				warn("write():");
		}

		if (likely(!done)) {
			if (unlikely(clock_gettime(CLOCK_MONOTONIC, &current) < 0))
				die("clock_gettime:");
			difftimespec(&diff, &current, &start);

			intspec.tv_sec = interval / 1000;
			intspec.tv_nsec = (interval % 1000) * 1E6;
			difftimespec(&wait, &intspec, &diff);

			if (unlikely(wait.tv_sec >= 0 &&
			             nanosleep(&wait, NULL) < 0 &&
			             errno != EINTR))
					die("nanosleep:");
		}
	} while (!done);

	return 0;
}
