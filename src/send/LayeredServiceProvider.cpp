#include "LayeredServiceProvider.h"

LayeredServiceProvider LayeredServiceProvider_Current;

LayeredServiceProvider::LayeredServiceProvider()
{
	filterguid = { 0xd3c21122, 0x85e1, 0x48f3,{ 0x9a,0xb6,0x23,0xd9,0x0c,0x73,0x07,0xef } };
	TotalProtos = 0;
	ProtoInfoSize = 0;
	ProtoInfo = NULL;
	StartProviderCompleted = NULL;
};

bool LayeredServiceProvider::Load()
{
	int error;
	ProtoInfo = NULL;
	ProtoInfoSize = 0;
	TotalProtos = 0;
	if (WSCEnumProtocols(NULL, ProtoInfo, &ProtoInfoSize, &error) == SOCKET_ERROR)
	{
		if (error != WSAENOBUFS)
		{
			Debugger::Write(L"First WSCEnumProtocols Error!");
			return FALSE;
		}
	}
	if ((ProtoInfo = (LPWSAPROTOCOL_INFOW)GlobalAlloc(GPTR, ProtoInfoSize)) == NULL)
	{
		Debugger::Write(L"GlobalAlloc Error!");
		return FALSE;
	}
	if ((TotalProtos = WSCEnumProtocols(NULL, ProtoInfo, &ProtoInfoSize, &error)) == SOCKET_ERROR)
	{
		Debugger::Write(L"Second WSCEnumProtocols Error!");
		return FALSE;
	}
	return TRUE;
}

void LayeredServiceProvider::Free()
{
	if (ProtoInfo != NULL && GlobalSize(ProtoInfo) > 0)
	{
		GlobalFree(ProtoInfo);
		ProtoInfo = NULL;
	}
}

int LayeredServiceProvider::Start(WORD wversionrequested,
	LPWSPDATA lpwspdata,
	LPWSAPROTOCOL_INFOW lpProtoInfo,
	WSPUPCALLTABLE upcalltable,
	LPWSPPROC_TABLE lpproctable)

{
	LayeredServiceProvider::Free();
	int i;
	int errorcode;
	int filterpathlen;
	DWORD layerid = 0;
	DWORD nextlayerid = 0;
	WCHAR *filterpath;
	HINSTANCE hfilter;
	LPWSPSTARTUP wspstartupfunc = NULL;
	if (lpProtoInfo->ProtocolChain.ChainLen <= 1)
	{
		Debugger::Write(L"ChainLen<=1");
		return FALSE;
	}
	LayeredServiceProvider::Load();
	for (i = 0; i < TotalProtos; i++)
	{
		if (memcmp(&ProtoInfo[i].ProviderId, &filterguid, sizeof(GUID)) == 0)
		{
			layerid = ProtoInfo[i].dwCatalogEntryId;
			break;
		}
	}
	for (i = 0; i < lpProtoInfo->ProtocolChain.ChainLen; i++)
	{
		if (lpProtoInfo->ProtocolChain.ChainEntries[i] == layerid)
		{
			nextlayerid = lpProtoInfo->ProtocolChain.ChainEntries[i + 1];
			break;
		}
	}
	filterpathlen = MAX_PATH;
	filterpath = (WCHAR*)GlobalAlloc(GPTR, filterpathlen);
	for (i = 0; i < TotalProtos; i++)
	{
		if (nextlayerid == ProtoInfo[i].dwCatalogEntryId)
		{
			if (WSCGetProviderPath(&ProtoInfo[i].ProviderId, filterpath, &filterpathlen, &errorcode) == SOCKET_ERROR)
			{
				Debugger::Write(L"WSCGetProviderPath Error!");
				return WSAEPROVIDERFAILEDINIT;
			}
			break;
		}
	}
	if (!ExpandEnvironmentStringsW(filterpath, filterpath, MAX_PATH))
	{
		Debugger::Write(L"ExpandEnvironmentStrings Error!");
		return WSAEPROVIDERFAILEDINIT;
	}
	if ((hfilter = LoadLibraryW(filterpath)) == NULL)
	{
		Debugger::Write(L"LoadLibrary Error!");
		return WSAEPROVIDERFAILEDINIT;
	}
	if ((wspstartupfunc = (LPWSPSTARTUP)GetProcAddress(hfilter, "WSPStartup")) == NULL)
	{
		Debugger::Write(L"GetProcessAddress Error!");
		return WSAEPROVIDERFAILEDINIT;
	}
	if ((errorcode = wspstartupfunc(wversionrequested, lpwspdata, lpProtoInfo, upcalltable, lpproctable)) != ERROR_SUCCESS)
	{
		Debugger::Write(L"wspstartupfunc Error!");
		return errorcode;
	}
	NextProcTable = *lpproctable; // 保存原来的入口函数表
	if (StartProviderCompleted != NULL)
	{
		StartProviderCompleted(&NextProcTable, lpproctable);
	}
	LayeredServiceProvider::Free();
	return 0;
};