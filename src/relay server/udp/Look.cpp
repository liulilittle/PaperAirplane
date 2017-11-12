#include "Look.h"

Look::Look()
{
	memset(&m_cs, 0, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&m_cs);
}

Look::~Look()
{
	DeleteCriticalSection(&m_cs);
}

bool Look::EnterLook()
{
	EnterCriticalSection(&m_cs);
	return true;
}

void Look::LeaveLook()
{
	LeaveCriticalSection(&m_cs);
}
