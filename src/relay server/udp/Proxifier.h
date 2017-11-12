#ifndef PROXIFIER_H
#define PROXIFIER_H

#include "Environment.h"
#include "SocketMappingPool.h"

int PRXGetAddressInfo(const struct sockaddr* addr, BYTE* name, USHORT& port, int& namelen);
BOOL PRXAddressToEndPoint(const struct sockaddr* name, BYTE* endpoint, INT prototype, int& len);
BOOL PRXEndPointToAddress(BYTE* endpoint, const struct sockaddr* name, INT& prototype, INT& len);
int PRXCreateTunnel(SOCKET s, const struct sockaddr* name, int namelen, INT socktype);
BOOL PRXCloseTunnel(SOCKET s, BOOL free);
int PRXGetSockaddrPort(const struct sockaddr* name);
BOOL PRXGetSockaddrAddress(const struct sockaddr* name, int namelen, BYTE* address);
int PRXGetSockaddrAddress(const struct sockaddr* name);
void PRXCloseAllTunnel(BOOL free);
SocketBinderContext* PRXCreateBinderContext(SOCKET s);
#endif