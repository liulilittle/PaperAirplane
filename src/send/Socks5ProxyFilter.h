#ifndef SOCKS5PROXYFILTER_H
#define SOCKS5PROXYFILTER_H

#include "Environment.h"

class Socks5ProxyFilter
{
public:
	static bool Effective(struct sockaddr_in* addr);

	static bool Effective(ULONG addr);

	static bool Effective(USHORT port);

	static bool Effective(string hostname);
};

#endif

