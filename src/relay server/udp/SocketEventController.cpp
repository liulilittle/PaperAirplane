#include "SocketEventController.h"
#include "LayeredServiceProvider.h"

extern LayeredServiceProvider LayeredServiceProvider_Current;
SocketEventController SocketEventController_Current;

DWORD SocketEventController::Handle(SocketEventController* self)
{
	vector<SetEventContext*> removes;
	do
	{
		self->Enter();
		{
			self->HandleSet(&removes);
		}
		self->Leave();
		Sleep(1);
	} while (self->m_hthread != NULL);
	return 0;
}

void SocketEventController::HandleSet(vector<SetEventContext*>* removes)
{
	for (size_t i = 0, len = m_contexts.size(); i < len; i++)
	{
		SetEventContext* context = m_contexts[i];
		if ((GetSysTickCount64() - context->CreateTick64) >= context->DelayTime)
		{
			removes->push_back(context);
			SetEvent(context->Socket, context->Binder, context->EventType);
		}
	}
	if (!removes->empty())
	{
		for each (SetEventContext* context in *removes)
		{
			for (size_t i = 0, len = m_contexts.size(); i < len; i++)
			{
				if (m_contexts[i] == context)
				{
					m_contexts.erase(m_contexts.begin() + i);
					delete context;
					break;
				}
			}
		}
		removes->clear();
	}
}

SocketEventController::SocketEventController()
{
	memset(&m_cs, 0, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&m_cs);

	DWORD threadid;
	m_hthread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&SocketEventController::Handle, this, 0, &threadid);
}

SocketEventController::~SocketEventController()
{
	if (m_hthread != NULL)
	{
		TerminateThread(m_hthread, 0);
		m_hthread = NULL;
		DeleteCriticalSection(&m_cs);
	}
}

inline void SocketEventController::Enter()
{
	EnterCriticalSection(&m_cs);
}

inline void SocketEventController::Leave()
{
	LeaveCriticalSection(&m_cs);
}

BOOL SocketEventController::SetEvent(SOCKET s, SocketBinderContext* binder, LONG eventtype, int delaytime)
{
	if (s == INVALID_SOCKET || binder == NULL || delaytime < 0)
	{
		return FALSE;
	}
	if (delaytime == 0)
	{
		return SetEvent(s, binder, eventtype);
	}

	SetEventContext* context = new SetEventContext;
	context->Socket = s;
	context->Binder = binder;
	context->EventType = eventtype;
	context->DelayTime = delaytime;
	context->CreateTick64 = GetSysTickCount64();

	Enter();
	m_contexts.push_back(context);
	Leave();
	return TRUE;
}

BOOL SocketEventController::SuspendEvent(SOCKET s, SocketBinderContext * binder)
{
	if (binder == NULL || s == INVALID_SOCKET)
	{
		return FALSE;
	}
	int err = 0; // THE CLEANUP BIND EVENT OBJECT
	if (LayeredServiceProvider_Current.NextProcTable.lpWSPEventSelect(s, 0, NULL, &err))
	{
		return FALSE;
	}
	binder->EnterLook();
	{
		for each(HWND hWnd in binder->HWndList)
		{
			err = 0;
			if (LayeredServiceProvider_Current.NextProcTable.lpWSPAsyncSelect(s, hWnd, 0, 0, &err) != NO_ERROR)
			{
				binder->LeaveLook();
				return FALSE; // WSAAsyncSelect(s, hWnd, 0, 0);
			}
		}
	}
	binder->LeaveLook();
	return TRUE;
}

BOOL SocketEventController::ResumeEvent(SOCKET s, SocketBinderContext * binder)
{
	if (binder == NULL || s == INVALID_SOCKET)
	{
		return FALSE;
	}
	binder->EnterLook();
	{
		vector<SocketEventContext*>* events = &binder->Events;
		for (size_t i = 0, len = events->size(); i < len; i++)
		{
			SocketEventContext* evt = (*events)[i];
			int err = 0;
			if (evt->hWnd == NULL)
			{
				if (LayeredServiceProvider_Current.NextProcTable.lpWSPEventSelect(s, evt->hEvent, evt->lNetworkEvents, &err))
				{
					return FALSE;
				}
			}
			else
			{
				if (LayeredServiceProvider_Current.NextProcTable.lpWSPAsyncSelect(s, evt->hWnd, evt->wMsg, evt->lNetworkEvents, &err))
				{
					return FALSE;
				}
			}
			delete evt;
		}
	}
	binder->LeaveLook();
	return TRUE;
}

BOOL SocketEventController::SetEvent(SOCKET s, SocketBinderContext * binder, LONG eventtype)
{
	if (s == INVALID_SOCKET || binder == NULL)
	{
		return FALSE;
	}
	binder->EnterLook();
	{
		vector<SocketEventContext*>* events = &binder->Events;
		for (size_t i = 0, len = events->size(); i < len; i++)
		{
			SocketEventContext* evt = (*events)[i];
			int err = 0;
			if (evt->lNetworkEvents & eventtype)
			{
				WSASetLastError(NO_ERROR);
				if (evt->hWnd == NULL)
					WSASetEvent(evt->hEvent);
				else
					SendMessageW(evt->hWnd, evt->wMsg, s, LOWORD(eventtype) | HIWORD(NO_ERROR));
			}
		}
	}
	binder->LeaveLook();
	return TRUE;
}

BOOL SocketEventController::ResetEvent(SOCKET s, SocketBinderContext * binder, LONG eventtype)
{
	if (s == INVALID_SOCKET || binder == NULL)
	{
		return FALSE;
	}
	binder->EnterLook();
	{
		vector<SocketEventContext*>* events = &binder->Events;
		for (size_t i = 0, len = events->size(); i < len; i++)
		{
			SocketEventContext* evt = (*events)[i];
			int err = 0;
			if (evt->lNetworkEvents & eventtype)
			{
				WSAResetEvent(evt->hEvent);
			}
		}
	}
	binder->LeaveLook();
	return TRUE;
}
