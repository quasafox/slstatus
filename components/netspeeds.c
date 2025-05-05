/* See LICENSE file for copyright and license details. */
#include <limits.h>
#include <stdio.h>

#include "../slstatus.h"
#include "../util.h"

#if defined(__linux__)
	#include <linux/limits.h>
	#include <sys/types.h>
	#include <stdint.h>
	#include <dirent.h>
	#include <errno.h>
	#include <fcntl.h>
	#include <stddef.h>
	#include <stdlib.h>
	#include <string.h>
	#include <unistd.h>

	#define NET_RX_BYTES "/sys/class/net/%s/statistics/rx_bytes"
	#define NET_TX_BYTES "/sys/class/net/%s/statistics/tx_bytes"

	char *detected_iface = NULL;

	static char *detect_up_iface(void)
	{
		DIR *dir;
		struct dirent *de;
		char buffer[PATH_MAX];
		char *iface = NULL;

		if (!(dir = opendir("/sys/class/net"))) {
			warn("opendir():");
			return NULL;
		}

		errno = 0;
		while ((de = readdir(dir))) {
			int fd;
			ssize_t nread;

			if (!de->d_name[0]
			    || de->d_name[0] == '.'
			    || !strncmp(de->d_name, "lo", 2))
				continue;

			if (snprintf(buffer,
				     sizeof(buffer),
				     "/sys/class/net/%s/operstate",
				     de->d_name) < 0) {
				warn("snprintf():");
				continue;
			}

			if ((fd = open(buffer, O_RDONLY)) == -1)
				continue;

			if ((nread = read(fd, buffer, sizeof(buffer)-1)) == -1)
				goto next;
			buffer[nread] = 0;

			if (!strncmp(buffer, "up", 2)) {
				iface = strdup(de->d_name);
				close(fd);
				break;
			}
next:
			close(fd);
		}

		if (errno)
			warn("readdir():");

		if (closedir(dir) == -1)
			warn("closedir():");

		return (detected_iface = iface);
	}

	const char *
	netspeed_rx(const char *interface)
	{
		uintmax_t oldrxbytes;
		static uintmax_t rxbytes;
		extern const unsigned int interval;
		char path[PATH_MAX];

		if (!interface) {
			if (!detected_iface && !detect_up_iface())
				return NULL;
			interface = detected_iface;
		}

		oldrxbytes = rxbytes;

		if (esnprintf(path, sizeof(path), NET_RX_BYTES, interface) < 0)
			return NULL;
		if (pscanf(path, "%ju", &rxbytes) != 1)
			goto redetect;
		if (oldrxbytes == 0)
			return NULL;

		return fmt_human((rxbytes - oldrxbytes) * 1000 / interval,
		                 1024);
redetect:
		free(detected_iface);
		detected_iface = NULL;
		return NULL;
	}

	const char *
	netspeed_tx(const char *interface)
	{
		uintmax_t oldtxbytes;
		static uintmax_t txbytes;
		extern const unsigned int interval;
		char path[PATH_MAX];

		if (!interface) {
			if (!detected_iface && !detect_up_iface())
				return NULL;
			interface = detected_iface;
		}

		oldtxbytes = txbytes;

		if (esnprintf(path, sizeof(path), NET_TX_BYTES, interface) < 0)
			return NULL;
		if (pscanf(path, "%ju", &txbytes) != 1)
			goto redetect;
		if (oldtxbytes == 0)
			return NULL;

		return fmt_human((txbytes - oldtxbytes) * 1000 / interval,
		                 1024);
redetect:
		free(detected_iface);
		detected_iface = NULL;
		return NULL;
	}
#elif defined(__OpenBSD__) | defined(__FreeBSD__)
	#include <ifaddrs.h>
	#include <net/if.h>
	#include <string.h>
	#include <sys/types.h>
	#include <sys/socket.h>

	const char *
	netspeed_rx(const char *interface)
	{
		struct ifaddrs *ifal, *ifa;
		struct if_data *ifd;
		uintmax_t oldrxbytes;
		static uintmax_t rxbytes;
		extern const unsigned int interval;
		int if_ok = 0;

		oldrxbytes = rxbytes;

		if (getifaddrs(&ifal) < 0) {
			warn("getifaddrs failed");
			return NULL;
		}
		rxbytes = 0;
		for (ifa = ifal; ifa; ifa = ifa->ifa_next)
			if (!strcmp(ifa->ifa_name, interface) &&
			   (ifd = (struct if_data *)ifa->ifa_data))
				rxbytes += ifd->ifi_ibytes, if_ok = 1;

		freeifaddrs(ifal);
		if (!if_ok) {
			warn("reading 'if_data' failed");
			return NULL;
		}
		if (oldrxbytes == 0)
			return NULL;

		return fmt_human((rxbytes - oldrxbytes) * 1000 / interval,
		                 1024);
	}

	const char *
	netspeed_tx(const char *interface)
	{
		struct ifaddrs *ifal, *ifa;
		struct if_data *ifd;
		uintmax_t oldtxbytes;
		static uintmax_t txbytes;
		extern const unsigned int interval;
		int if_ok = 0;

		oldtxbytes = txbytes;

		if (getifaddrs(&ifal) < 0) {
			warn("getifaddrs failed");
			return NULL;
		}
		txbytes = 0;
		for (ifa = ifal; ifa; ifa = ifa->ifa_next)
			if (!strcmp(ifa->ifa_name, interface) &&
			   (ifd = (struct if_data *)ifa->ifa_data))
				txbytes += ifd->ifi_obytes, if_ok = 1;

		freeifaddrs(ifal);
		if (!if_ok) {
			warn("reading 'if_data' failed");
			return NULL;
		}
		if (oldtxbytes == 0)
			return NULL;

		return fmt_human((txbytes - oldtxbytes) * 1000 / interval,
		                 1024);
	}
#endif
