#include "Environment.h"
#include "Proxifier.h"

BOOL WINAPI DllMain(HINSTANCE hmodule,
	DWORD reason,
	LPVOID lpreserved)
{
	switch (reason)
	{ 
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	};
	return TRUE;
}