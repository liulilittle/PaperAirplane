#ifndef SOCKETMAPPINGPOOL_H
#define SOCKETMAPPINGPOOL_H

#include "Environment.h"

typedef struct SocketEventContext
{
	WSAEVENT hEvent;
	LONG lNetworkEvents;
	HWND hWnd;
	UINT wMsg;
} SocketEventContext;

typedef struct SocketBinderContext
{
	SOCKET SocketHandle;
	INT BindPort;
	inline void ctor();
} SocketBinderContext;

class SocketMappingPool
{
private:
	hash_map<SOCKET, SocketBinderContext*>* m_mapTable;
	CRITICAL_SECTION m_cs;

public:
	SocketMappingPool();

	~SocketMappingPool();

	void Enter();

	void Leave();

	hash_map<SOCKET, SocketBinderContext*>* Container();

	SocketBinderContext* Add(SOCKET s);

	SocketBinderContext* Add(SocketBinderContext* context);

	SocketBinderContext* Get(SOCKET s);

	SocketBinderContext* Remove(SOCKET s);

	SocketBinderContext* Remove(SocketBinderContext* context);

	void RemoveAll();

	bool ContainsKey(SOCKET s);
};
#endif
