#ifndef SOCKETEVENTCONTROLLER_H
#define SOCKETEVENTCONTROLLER_H

#include "Environment.h"
#include "SocketMappingPool.h"

class SocketEventController
{
	typedef struct SetEventContext
	{
		SOCKET Socket;
		SocketBinderContext* Binder;
		LONG EventType;
		int DelayTime;
		INT64 CreateTick64;
	} Context;

private:
	CRITICAL_SECTION m_cs;
	HANDLE m_hthread;
	vector<SocketEventController::SetEventContext*> m_contexts;

	static DWORD WINAPI Handle(SocketEventController* state);

	void HandleSet(vector<SetEventContext*>* removes);
public:
	SocketEventController();
	~SocketEventController();

	void Enter();
	void Leave();

	BOOL SetEvent(SOCKET s, SocketBinderContext* binder, LONG eventtype, int delaytime);
	BOOL SuspendEvent(SOCKET s, SocketBinderContext* binder);
	BOOL ResumeEvent(SOCKET s, SocketBinderContext* binder);
	BOOL SetEvent(SOCKET s, SocketBinderContext* binder, LONG eventtype);
	BOOL ResetEvent(SOCKET s, SocketBinderContext* binder, LONG eventtype);
};
#endif

