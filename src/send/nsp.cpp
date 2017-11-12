#include "Environment.h"
#include "Debugger.h"
#include "StringExtension.h"
#include "SupersocksRInteractive.h"
#include "NamespaceMappingTable.h"
#include "NamespaceServiceProvider.h"

extern NamespaceServiceProvider NamespaceServiceProvider_Current;
extern NamespaceMappingTable NamespaceMappingTable_Current;
extern SupersocksRInteractive SupersocksRInteractive_Current;

UINT RAND_MAP_IPADDRESS_NUM = 0x7F080000;

typedef struct NSPLookupContext
{
	LPWSTR hostname;
	LPCSTR hostaddr;
	BYTE behavior;
} NSPLookupContext;

int WINAPI NSPLookupServiceNext(HANDLE hLookup, DWORD dwControlFlags, LPDWORD lpdwBufferLength, LPWSAQUERYSETW lpqsResults)
{
	SupersocksRConfiguration* conf = SupersocksRInteractive_Current.Configuration();
	if (!(conf->EnableProxyClient && conf->ResolveDNSRemote))
	{
		return NamespaceServiceProvider_Current.NSP_ROUTINE.NSPLookupServiceNext(hLookup, dwControlFlags, lpdwBufferLength, lpqsResults);
	}
	NSPLookupContext* context = (NSPLookupContext*)hLookup;
	if (context == NULL || IsBadReadPtr(context, sizeof(NSPLookupContext)) || context->behavior >= 1)
	{
		return SOCKET_ERROR;
	}
	LPCSTR hostaddr = StringExtension::W2A(context->hostname);
	context->hostaddr = hostaddr;
	if (hostaddr == NULL)
	{
		return SOCKET_ERROR;
	}
	else
	{
		strlwr((char*)hostaddr);
	}
	memset(lpqsResults, 0, sizeof(WSAQUERYSETW));
	lpqsResults->dwSize = 120;
	lpqsResults->dwNameSpace = NS_DNS;
	lpqsResults->dwNumberOfProtocols = 0;
	lpqsResults->dwNumberOfCsAddrs = 1;
	lpqsResults->dwOutputFlags = 0;
	lpqsResults->lpszQueryString = context->hostname;
	lpqsResults->lpszServiceInstanceName = context->hostname;
	lpqsResults->lpcsaBuffer = new CSADDR_INFO({ 0 });

	struct sockaddr_in* localEP = new struct sockaddr_in({ 0 });
	localEP->sin_family = AF_INET;

	struct sockaddr_in* remoteEP = new struct sockaddr_in({ 0 });
	remoteEP->sin_family = AF_INET;

	ULONG address = 0;
	if (NamespaceMappingTable_Current.ContainsKey(hostaddr))
	{
		address = NamespaceMappingTable_Current.Get(hostaddr);
		remoteEP->sin_addr.s_addr = address;
	}
	else
	{
		address = InterlockedIncrement(&RAND_MAP_IPADDRESS_NUM);
		address = ntohl(address);
		remoteEP->sin_addr.s_addr = address;

		NamespaceMappingTable_Current.Add(address, hostaddr);
	}
	CSADDR_INFO* addr = lpqsResults->lpcsaBuffer;

	addr->iSocketType = SOCK_RAW;
	addr->iProtocol = 23;

	addr->RemoteAddr.iSockaddrLength = sizeof(struct sockaddr_in);
	addr->LocalAddr.iSockaddrLength = sizeof(struct sockaddr_in);
	addr->LocalAddr.lpSockaddr = (struct sockaddr*)localEP;
	addr->RemoteAddr.lpSockaddr = (struct sockaddr*)remoteEP;

	context->behavior = 1;
	return NO_ERROR;
}

int WINAPI NSPLookupServiceBegin(LPGUID lpProviderId, LPWSAQUERYSETW lpqsRestrictions, LPWSASERVICECLASSINFOW lpServiceClassInfo, DWORD dwControlFlags, LPHANDLE lphLookup)
{
	SupersocksRConfiguration* conf = SupersocksRInteractive_Current.Configuration();
	if (conf->EnableProxyClient && conf->ResolveDNSRemote)
	{
		if (lpqsRestrictions == NULL || lphLookup == NULL || lpqsRestrictions->lpszServiceInstanceName == NULL)
		{
			return SOCKET_ERROR;
		}
		NSPLookupContext* context = new NSPLookupContext({ 0 });
		context->hostname = lpqsRestrictions->lpszServiceInstanceName;
		*lphLookup = (HANDLE)context;
		return NO_ERROR;
	}
	return NamespaceServiceProvider_Current.NSP_ROUTINE.NSPLookupServiceBegin(lpProviderId, lpqsRestrictions, lpServiceClassInfo, dwControlFlags, lphLookup);
}

int WINAPI NSPLookupServiceEnd(HANDLE hLookup)
{
	SupersocksRConfiguration* conf = SupersocksRInteractive_Current.Configuration();
	if (conf->EnableProxyClient && conf->ResolveDNSRemote)
	{
		NSPLookupContext* context = (NSPLookupContext*)hLookup;
		if (context == NULL || IsBadReadPtr(context, sizeof(NSPLookupContext)))
		{
			return SOCKET_ERROR;
		}
		context->behavior = 2;
		delete context;
		return NO_ERROR;
	}
	return NamespaceServiceProvider_Current.NSP_ROUTINE.NSPLookupServiceEnd(hLookup);
}

void WSPAPI StartedNspCompleted(NSP_ROUTINE* sender, NSP_ROUTINE* e)
{
	e->NSPLookupServiceNext = NSPLookupServiceNext;
	e->NSPLookupServiceBegin = NSPLookupServiceBegin;
	e->NSPLookupServiceEnd = NSPLookupServiceEnd;
}

int WSPAPI NSPStartup (  
    LPGUID lpProviderId,
    LPNSP_ROUTINE lpnspRoutines)
{
	TCHAR processname[MAX_PATH];
	GetModuleFileName(NULL, processname, MAX_PATH);
	Debugger::Write(L"%s Loading NSPStartup ...", processname);

	NamespaceServiceProvider_Current.StartedNspCompleted = &StartedNspCompleted;
	return NamespaceServiceProvider_Current.Start(lpProviderId, lpnspRoutines);
}
