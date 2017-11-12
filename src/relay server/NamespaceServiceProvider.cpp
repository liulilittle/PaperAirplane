#include "NamespaceServiceProvider.h"

NamespaceServiceProvider NamespaceServiceProvider_Current;

int NamespaceServiceProvider::Start(LPGUID lpProviderId, LPNSP_ROUTINE lpnspRoutines)
{
	LPNSPSTARTUP        NSPStartupFunc = NULL;
	HMODULE				hLibraryHandle = NULL;
	INT                 ErrorCode = 0;

	if ((hLibraryHandle = LoadLibraryW(L"mswsock.dll")) == NULL
		|| (NSPStartupFunc = (LPNSPSTARTUP)GetProcAddress(
			hLibraryHandle, "NSPStartup")) == NULL)
	{
		return SOCKET_ERROR;
	}
	if ((ErrorCode = NSPStartupFunc(lpProviderId, lpnspRoutines)) != NO_ERROR)
	{
		return ErrorCode;
	}
	NSP_ROUTINE = *lpnspRoutines;
	if (StartedNspCompleted != NULL)
	{
		StartedNspCompleted(&NSP_ROUTINE, lpnspRoutines);
	}
	return NO_ERROR;
}
