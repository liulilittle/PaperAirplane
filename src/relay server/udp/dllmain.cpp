#include "Environment.h"
#include "PaperAirplaneInteractive.h"
#include "LayeredServiceProvider.h"
#include "SocketMappingPool.h"

#include "Proxifier.h"

extern LayeredServiceProvider LayeredServiceProvider_Current;
extern SocketMappingPool SocketMappingPool_Current;
extern PaperAirplaneInteractive PaperAirplaneInteractive_Current;

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
};
