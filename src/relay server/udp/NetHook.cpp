#include "NetHook.h"

NetHook::NetHook()
{
	m_asmsize = (sizeof(VOID*) > 4 ? 12 : 5);
	m_oldmemprpt = 0;

	m_newasm = new BYTE[m_asmsize];
	m_oldasm = new BYTE[m_asmsize];
	memset(m_newasm, 0, m_asmsize);
	memset(m_oldasm, 0, m_asmsize);

	memset(&m_cs, 0, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&m_cs);
}

NetHook::~NetHook()
{
	if (m_newasm != NULL)
	{
		this->Suspend();
		delete[] m_newasm;
		delete[] m_oldasm;
		DeleteCriticalSection(&m_cs);
	}
}

void NetHook::EnterLook()
{
	EnterCriticalSection(&m_cs);
}

void NetHook::LeaveLook()
{
	LeaveCriticalSection(&m_cs);
}

BOOL NetHook::Install(VOID * oldfunc, VOID * newfunc)
{
	this->EnterLook();
	if (oldfunc == NULL || newfunc == NULL)
	{
		Debugger::Write("oldfunc is null %d, newfun is null %d", oldfunc == NULL, newfunc == NULL);
		this->LeaveLook();
		return FALSE;
	}
	m_oldfunc = oldfunc;
	m_newfunc = newfunc;
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
	if (!VirtualProtectEx(hProcess, oldfunc, m_asmsize, PAGE_EXECUTE_READWRITE, &m_oldmemprpt))
	{
		Debugger::Write("修改内存保护失败：asmsize=%d, oldfunc=%d, newfun=%d", m_asmsize, oldfunc, newfunc);
		CloseHandle(hProcess);
		this->LeaveLook();
		return FALSE;
	}
	CloseHandle(hProcess);
	memcpy(m_oldasm, oldfunc, m_asmsize);
	if (m_asmsize > 5)
	{
		m_newasm[0] = 0x48; // MOV
		m_newasm[1] = 0xB8; // RAX
		*(LONGLONG*)&m_newasm[2] = *(LONGLONG*)newfunc; // PVA
		m_newasm[10] = 0xFF; // JMP
		m_newasm[11] = 0xE0; // RAX
	}
	else
	{
#pragma warning(disable: 4311)
#pragma warning(disable: 4302)
		m_newasm[0] = 0xE9; // JMP
		*reinterpret_cast<UINT*>(m_newasm + 1) = ((UINT)newfunc - ((UINT)oldfunc + 5)); // RVA
#pragma warning(default: 4311)
#pragma warning(disable: 4302)
	}
	this->Resume();
	this->LeaveLook();
	return TRUE;
}

void NetHook::Resume()
{
	this->EnterLook();
	memcpy(m_oldfunc, m_newasm, m_asmsize); // RESTORE MEMORY PROTECT
	this->LeaveLook();
}

void NetHook::Suspend()
{
	this->EnterLook();
	memcpy(m_oldfunc, m_oldasm, m_asmsize); // RESTORE MEMORY PROTECT
	this->LeaveLook();
}
