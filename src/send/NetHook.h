#ifndef NETHOOK_H
#define NETHOOK_H

#include "Environment.h"
#include "Debugger.h"

class NetHook
{
private:
	BYTE* m_newasm;
	BYTE* m_oldasm;
	VOID* m_oldfunc;
	VOID* m_newfunc;
	INT m_asmsize;
	DWORD m_oldmemprpt;
	CRITICAL_SECTION m_cs;

	void EnterLook();

	void LeaveLook();

public:
	NetHook();

	~NetHook();

	BOOL Install(VOID* oldfunc, VOID* newfunc);

	void Resume();

	void Suspend();
};
#endif