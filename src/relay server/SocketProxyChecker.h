#ifndef SOCKETPROXYCHECKER_H
#define SOCKETPROXYCHECKER_H

#include "Environment.h"

class SocketProxyChecker
{
public:
	static bool Effective(struct sockaddr_in* addr);

	static bool Effective(ULONG addr);

	static bool Effective(USHORT port);

	static bool Effective(string hostname);
};

#endif

