#ifndef LAYERED_SERVICE_PROVIDER_H
#define LAYERED_SERVICE_PROVIDER_H

#include "Environment.h"

typedef void (WSPAPI* StartLspCompletedEventHandler)(WSPPROC_TABLE* sender, WSPPROC_TABLE* e);

class LayeredServiceProvider
{
private:
	WSAPROTOCOL_INFOW* ProtoInfo;
	DWORD ProtoInfoSize;
	int TotalProtos;
	GUID filterguid;
	CRITICAL_SECTION cs;

	// 获取各种值
	bool Load();

	void Free(); // 释放内存

public:
	StartLspCompletedEventHandler StartProviderCompleted;
	WSPPROC_TABLE NextProcTable;

	inline void Enter()
	{
		EnterCriticalSection(&cs);
	}

	inline void Leave()
	{
		LeaveCriticalSection(&cs);
	}

	LayeredServiceProvider();

	~LayeredServiceProvider();

	int Start(WORD wversionrequested,
		LPWSPDATA lpwspdata,
		LPWSAPROTOCOL_INFOW lpProtoInfo,
		WSPUPCALLTABLE upcalltable,
		LPWSPPROC_TABLE lpproctable);
};

#endif
