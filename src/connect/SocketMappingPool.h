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
	BYTE* PeerName;
	INT PeerNameLen;
	ADDRESS_FAMILY AddressFrmily;
	CRITICAL_SECTION Look;
	BOOL Nonblock;
	vector<SocketEventContext*> Events;
	hash_set<HWND> HWndList;

	inline void ctor();

	inline void EnterLook()
	{
		EnterCriticalSection(&this->Look);
	}

	inline void LeaveLook()
	{
		LeaveCriticalSection(&this->Look);
	}

	inline void Finalize()
	{
		DeleteCriticalSection(&this->Look);
	}
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
