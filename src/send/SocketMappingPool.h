#ifndef SOCKETMAPPINGPOOL_H
#define SOCKETMAPPINGPOOL_H

#include "Environment.h"

typedef struct SocketEventContext
{
	__int64 hEvent;
	__int32 lNetworkEvents;
	HWND hWnd;
	unsigned __int32 wMsg;
} SocketEventContext;

typedef struct SocketBinderContext
{
	SOCKET SocketHandle;
	BYTE* PeerName;
	INT PeerNameLen;
	ADDRESS_FAMILY AddressFrmily;
	BOOL RequireHandshake;
	CRITICAL_SECTION* Look;
	BOOL Nonblock;
	vector<SocketEventContext*> Events;
	hash_set<HWND> HWndList;

	void ctor();

	void EnterLook();

	void LeaveLook();

	void Finalize();
} SocketBinderContext;

class SocketMappingPool
{
private:
	hash_map<SOCKET, SocketBinderContext*>* m_mapTable;
	CRITICAL_SECTION m_cs;

	void Enter();

	void Leave();

public:
	SocketMappingPool();

	~SocketMappingPool();

	SocketBinderContext* Add(SOCKET s);

	SocketBinderContext* Add(SocketBinderContext* context);

	SocketBinderContext* Get(SOCKET s);

	SocketBinderContext* Remove(SOCKET s);

	SocketBinderContext* Remove(SocketBinderContext* context);

	void RemoveAll();

	bool ContainsKey(SOCKET s);
};
#endif
