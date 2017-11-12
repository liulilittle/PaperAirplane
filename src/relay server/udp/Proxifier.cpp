#include "Proxifier.h"
#include "PaperAirplaneInteractive.h"
#include "NamespaceMappingTable.h"
#include "PaperAirplaneInteractive.h"
#include "SocketMappingPool.h"

extern NamespaceMappingTable NamespaceMappingTable_Current;
extern PaperAirplaneInteractive PaperAirplaneInteractive_Current;
extern SocketMappingPool SocketMappingPool_Current;

int PRXGetAddressInfo(const struct sockaddr* addr, BYTE* name, USHORT& port, int& namelen) // 打包地址
{
	BYTE atype = 0;
	namelen = 0;
	if (addr->sa_family != AF_INET)
	{
		struct sockaddr_in6* sin = (struct sockaddr_in6*)addr;
		port = sin->sin6_port;
		namelen = 16;
		memcpy(name, &sin->sin6_addr.u.Byte, 16);
		atype |= PAPERAIRPLANE_ADDRESSFRAMILY_INET6;
	}
	else
	{
		struct sockaddr_in* sin = (struct sockaddr_in*)addr;
		port = sin->sin_port;
		LPCSTR hostname = NamespaceMappingTable_Current.Get(sin->sin_addr.s_addr);
		if (hostname == NULL)
		{
			namelen = 4;
			memcpy(name, &sin->sin_addr.s_addr, 4);
			atype |= PAPERAIRPLANE_ADDRESSFRAMILY_INET;
		}
		else
		{
			namelen = (int)strlen(hostname);
			memcpy(&name[1], hostname, namelen++);
			name[1 + namelen] = 0;
			name[0] = namelen;
			atype |= PAPERAIRPLANE_ADDRESSFRAMILY_HOSTNAME;
		}
	}
	return atype;
}

BOOL PRXAddressToEndPoint(const struct sockaddr* name, BYTE* endpoint, INT prototype, int& len)
{
	if (name == NULL || endpoint == NULL)
		return FALSE;
	USHORT port;
	len = 0;
	endpoint[0] = (PRXGetAddressInfo(name, &endpoint[1], port, len) | prototype);
	memcpy(&endpoint[++len], &port, 2);
	len += 2;
	return TRUE;
}

BOOL PRXEndPointToAddress(BYTE* endpoint, const struct sockaddr* name, INT& prototype, INT& len)
{
	if (name == NULL || endpoint == NULL)
		return FALSE;
	int atype = endpoint[0];

	if (atype & PAPERAIRPLANE_PROTOCOLTYPE_UDP)
		prototype = PAPERAIRPLANE_PROTOCOLTYPE_UDP;
	else if (atype & PAPERAIRPLANE_PROTOCOLTYPE_TCP)
		prototype = PAPERAIRPLANE_PROTOCOLTYPE_TCP;
	else
		return FALSE;
	int offset = 1;
	if (atype & PAPERAIRPLANE_ADDRESSFRAMILY_INET) {
		struct sockaddr_in* sin = (struct sockaddr_in*)name;
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = *(ULONG*)&endpoint[offset];
		sin->sin_port = *(USHORT*)&endpoint[(offset += 4)];
		len = 4;
	}
	else if (atype & PAPERAIRPLANE_ADDRESSFRAMILY_INET6) {
		struct sockaddr_in6* sin6 = (struct sockaddr_in6*)name;
		sin6->sin6_family = AF_INET6;
		memcpy(sin6->sin6_addr.u.Byte, &endpoint[offset], 16);
		sin6->sin6_port = *(USHORT*)endpoint[(offset += 16)];
		len = 16;
	}
	else if (atype & PAPERAIRPLANE_ADDRESSFRAMILY_HOSTNAME) {
		int size = endpoint[offset++];
		ULONG address = NamespaceMappingTable_Current.Get(reinterpret_cast<LPCSTR>(&endpoint[offset]));
		if (address == 0) {
			address = htonl(INADDR_LOOPBACK);
		}
		struct sockaddr_in* sin = (struct sockaddr_in*)name;
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = address;
		sin->sin_port = *reinterpret_cast<USHORT*>(&endpoint[(offset += size)]);
		len = size;
	}
	return TRUE;
}

int PRXCreateTunnel(SOCKET s, const struct sockaddr* name, int namelen, INT socktype) // 创建隧道
{
	if (socktype == SOCK_STREAM) {
		BYTE address[MAXBYTE];
		USHORT port = 0;
		int len = 0;
		int outport;
		int atype = PRXGetAddressInfo(name, address, port, len);
		atype |= PAPERAIRPLANE_PROTOCOLTYPE_TCP;
		if (PaperAirplaneInteractive_Current.ProxyConnect(s, address, len, port, atype, &outport)) {
			return outport;
		}
	}
	else if (socktype == SOCK_DGRAM) {
		int outport;
		if (PaperAirplaneInteractive_Current.ProxyBind(s, &outport)) {
			return outport;
		}
	}
	return 0;
}

BOOL PRXCloseTunnel(SOCKET s, BOOL free)
{
	if (free) {
		SocketBinderContext* context = SocketMappingPool_Current.Remove(s);
		if (context != NULL) {
			delete context;
		}
	}
	return PaperAirplaneInteractive_Current.ProxyClose(s);
}

int PRXGetSockaddrPort(const struct sockaddr* name)
{
	if (name == NULL)
	{
		return 0;
	}
	if (name->sa_family != AF_INET)
	{
		struct sockaddr_in6* sin = (struct sockaddr_in6*)name;
		return sin->sin6_port;
	}
	else
	{
		struct sockaddr_in* sin = (struct sockaddr_in*)name;
		return sin->sin_port;
	}
}

BOOL PRXGetSockaddrAddress(const struct sockaddr* name, int namelen, BYTE* address)
{
	if (name == NULL || namelen <= 0 || address == NULL)
		return FALSE;
	if (name->sa_family != AF_INET)
	{
		struct sockaddr_in6* sin = (struct sockaddr_in6*)name;
		memcpy(address, &sin->sin6_addr.u.Byte, 16);
		return TRUE;
	}
	else
	{
		struct sockaddr_in* sin = (struct sockaddr_in*)name;
		memcpy(address, &sin->sin_addr.s_addr, 4);
		return TRUE;
	}
}

int PRXGetSockaddrAddress(const struct sockaddr* name)
{
	if (name == NULL)
		return 0;
	struct sockaddr_in* addr = (struct sockaddr_in*)name;
	return addr->sin_addr.s_addr;
}

void PRXCloseAllTunnel(BOOL free)
{
	SocketMappingPool_Current.Enter();
	{
		hash_map<SOCKET, SocketBinderContext*>* container = SocketMappingPool_Current.Container();
		hash_map<SOCKET, SocketBinderContext*>::iterator it = container->begin();
		for (; it != container->end(); it++)
		{
			SocketBinderContext* context = (*it).second;
			if (free) {
				delete context;
			}
			PRXCloseTunnel((*it).first, FALSE);
		}
		if (free) {
			SocketMappingPool_Current.RemoveAll();
		}
	}
	SocketMappingPool_Current.Leave();
}

SocketBinderContext* PRXCreateBinderContext(SOCKET s)
{
	if (s == INVALID_SOCKET) {
		return NULL;
	}
	SocketMappingPool_Current.Enter();
	SocketBinderContext* context = SocketMappingPool_Current.Get(s);
	if (context == NULL) {
		int port = PRXCreateTunnel(s, NULL, NULL, SOCK_DGRAM);
		if (port != 0) {
			context = SocketMappingPool_Current.Add(s);
			if (context != NULL) {
				context->BindPort = port;
			}
			else {
				PRXCloseTunnel(s, FALSE);
			}
		}
	}
	SocketMappingPool_Current.Leave();
	return context;
}