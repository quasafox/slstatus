/* See LICENSE file for copyright and license details. */
#include <stdio.h>

#include "../slstatus.h"
#include "../util.h"

#if defined(__linux__)
	#include <stdint.h>
	#include <stddef.h>
	#include <assert.h>
	#include "../util-aux.h"

	const char *
	ram_free(const char *unused)
	{
		uintmax_t free;

		if (pscanf("/proc/meminfo",
		           "MemTotal: %ju kB\n"
		           "MemFree: %ju kB\n"
		           "MemAvailable: %ju kB\n",
		           &free, &free, &free) != 3)
			return NULL;

		return fmt_human(free * 1024, 1024);
	}

	const char *
	ram_perc(const char *unused)
	{
		uintmax_t total, free, buffers, cached;
		int percent;

		if (pscanf("/proc/meminfo",
		           "MemTotal: %ju kB\n"
		           "MemFree: %ju kB\n"
		           "MemAvailable: %ju kB\n"
		           "Buffers: %ju kB\n"
		           "Cached: %ju kB\n",
		           &total, &free, &buffers, &buffers, &cached) != 5)
			return NULL;

		if (total == 0)
			return NULL;

		percent = 100 * ((total - free) - (buffers + cached)) / total;
		return bprintf("%d", percent);
	}

	const char *
	ram_total(const char *unused)
	{
		uintmax_t total;

		if (pscanf("/proc/meminfo", "MemTotal: %ju kB\n", &total)
		    != 1)
			return NULL;

		return fmt_human(total * 1024, 1024);
	}

	const char *
	ram_used(const char *unused)
	{
		static char readbuf[4096];
		size_t nread = sizeof(readbuf);
		size_t memtotal, memfree, buffers, cached, srecl;

		if (unlikely(!read_whole_file("/proc/meminfo", readbuf, &nread)))
			return NULL;

		char *line;
		size_t linelen;
		char found = 5;
		struct line_iter iter = LINE_ITER(readbuf, nread);
		while (likely(line_iter_next(&iter, &line, &linelen)) && found) {
			struct split_string split;
			uint64_t hash;
			size_t *target;

			if (!split_string(line, linelen, ':', &split))
				ALWAYS_UNREACHABLE();

			if (split.xlen < 6 || split.xlen > 12)
				continue;

			hash = hash_bytes(split.x, split.xlen);

			if (hash == hash_bytes("MemTotal", 8))
				target = &memtotal;
			else if (hash == hash_bytes("MemFree", 7))
				target = &memfree;
			else if (hash == hash_bytes("Buffers", 7))
				target = &buffers;
			else if (hash == hash_bytes("Cached", 6))
				target = &cached;
			else if (hash == hash_bytes("SReclaimable", 12))
				target = &srecl;
			else
				continue;

			*target = parse_size_t(trim_string_left(split.y, &split.ylen));
			--found;
		}

		return fmt_human((memtotal - memfree - buffers - cached - srecl) * 1024, 1024);
	}
#elif defined(__OpenBSD__)
	#include <stdlib.h>
	#include <sys/sysctl.h>
	#include <sys/types.h>
	#include <unistd.h>

	#define LOG1024 10
	#define pagetok(size, pageshift) (size_t)(size << (pageshift - LOG1024))

	inline int
	load_uvmexp(struct uvmexp *uvmexp)
	{
		int uvmexp_mib[] = {CTL_VM, VM_UVMEXP};
		size_t size;

		size = sizeof(*uvmexp);

		if (sysctl(uvmexp_mib, 2, uvmexp, &size, NULL, 0) >= 0)
			return 1;

		return 0;
	}

	const char *
	ram_free(const char *unused)
	{
		struct uvmexp uvmexp;
		int free_pages;

		if (!load_uvmexp(&uvmexp))
			return NULL;

		free_pages = uvmexp.npages - uvmexp.active;
		return fmt_human(pagetok(free_pages, uvmexp.pageshift) *
				 1024, 1024);
	}

	const char *
	ram_perc(const char *unused)
	{
		struct uvmexp uvmexp;
		int percent;

		if (!load_uvmexp(&uvmexp))
			return NULL;

		percent = uvmexp.active * 100 / uvmexp.npages;
		return bprintf("%d", percent);
	}

	const char *
	ram_total(const char *unused)
	{
		struct uvmexp uvmexp;

		if (!load_uvmexp(&uvmexp))
			return NULL;

		return fmt_human(pagetok(uvmexp.npages,
					 uvmexp.pageshift) * 1024, 1024);
	}

	const char *
	ram_used(const char *unused)
	{
		struct uvmexp uvmexp;

		if (!load_uvmexp(&uvmexp))
			return NULL;

		return fmt_human(pagetok(uvmexp.active,
					 uvmexp.pageshift) * 1024, 1024);
	}
#elif defined(__FreeBSD__)
	#include <sys/sysctl.h>
	#include <sys/vmmeter.h>
	#include <unistd.h>
	#include <vm/vm_param.h>

	const char *
	ram_free(const char *unused) {
		struct vmtotal vm_stats;
		int mib[] = {CTL_VM, VM_TOTAL};
		size_t len;

		len = sizeof(struct vmtotal);
		if (sysctl(mib, 2, &vm_stats, &len, NULL, 0) < 0
		    || !len)
			return NULL;

		return fmt_human(vm_stats.t_free * getpagesize(), 1024);
	}

	const char *
	ram_total(const char *unused) {
		unsigned int npages;
		size_t len;

		len = sizeof(npages);
		if (sysctlbyname("vm.stats.vm.v_page_count",
		                 &npages, &len, NULL, 0) < 0 || !len)
			return NULL;

		return fmt_human(npages * getpagesize(), 1024);
	}

	const char *
	ram_perc(const char *unused) {
		unsigned int npages;
		unsigned int active;
		size_t len;

		len = sizeof(npages);
		if (sysctlbyname("vm.stats.vm.v_page_count",
		                 &npages, &len, NULL, 0) < 0 || !len)
			return NULL;

		if (sysctlbyname("vm.stats.vm.v_active_count",
		                 &active, &len, NULL, 0) < 0 || !len)
			return NULL;

		return bprintf("%d", active * 100 / npages);
	}

	const char *
	ram_used(const char *unused) {
		unsigned int active;
		size_t len;

		len = sizeof(active);
		if (sysctlbyname("vm.stats.vm.v_active_count",
		                 &active, &len, NULL, 0) < 0 || !len)
			return NULL;

		return fmt_human(active * getpagesize(), 1024);
	}
#endif
