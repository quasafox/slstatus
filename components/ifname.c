#include "netspeeds.h"
#include "../slstatus.h"

const char *ifname(const char *unused)
{
	(void)unused;

	return detected_iface ? detected_iface : "?";
}
