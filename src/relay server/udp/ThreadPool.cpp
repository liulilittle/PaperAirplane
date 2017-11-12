#include "ThreadPool.h"

#ifndef SHUTDOWN_IOCPTHREAD
#define SHUTDOWN_IOCPTHREAD 0x7fffffff
#endif
#ifndef INIFINITE
#define INIFINITE -1
#endif

typedef struct ThreadPoolContext
{
	LPTHREADPOOLWAITCALLBACK Callback;
	LPVOID State;
} ThreadPoolContext;

ThreadPool::ThreadPool(int iMinConcurrency, int iMaxConcurrency)
{
	m_iMaxThreadsInPool = iMaxConcurrency;
	m_iActThreadsInPool = 0;
	m_iCurWorkInPool = 0;
	m_iCurThreadsInPool = 0;
	M_iMinThreadsInPool = iMinConcurrency;

	m_hIoCompPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, iMaxConcurrency);
	for (int i = 0; i < iMinConcurrency; i++)
	{
		StartNewThread();
	}
}

DWORD WINAPI ThreadPool::Queue(ThreadPool* self)
{
	DWORD uiNumberOfBytes;
	ULONG_PTR dwCompletionKey;
	LPOVERLAPPED lpOverlapped;
	while (self->m_hIoCompPort)
	{
		GetQueuedCompletionStatus(self->m_hIoCompPort, &uiNumberOfBytes, &dwCompletionKey, &lpOverlapped, INIFINITE);
		if (uiNumberOfBytes <= 0 || lpOverlapped == NULL)
		{
			continue;
		}
		self->DecCurWorkInPool();
		if (dwCompletionKey == SHUTDOWN_IOCPTHREAD)
		{
			break;
		}
		self->IncActThreadsInPool();

		ThreadPoolContext* context = (ThreadPoolContext*)lpOverlapped;
		context->Callback(context->State);
		LocalFree(context);

		if (self->m_iCurThreadsInPool < self->m_iMaxThreadsInPool && self->m_iActThreadsInPool == self->m_iCurThreadsInPool)
		{
			self->StartNewThread();
		}
		self->DecActThreadsInPool();
	}
	self->DecCurThreadsInPool();
	return 0;
}

void ThreadPool::QueueUserWork(LPTHREADPOOLWAITCALLBACK callback, void* state)
{
	ThreadPool* self = this;
	if (m_hIoCompPort && callback && state)
	{
		ThreadPoolContext* context = (ThreadPoolContext*)LocalAlloc(LMEM_FIXED, sizeof(ThreadPoolContext));
		context->Callback = callback;
		context->State = state;
		PostQueuedCompletionStatus(m_hIoCompPort, sizeof(ThreadPoolContext), NULL, (LPOVERLAPPED)context);

		self->IncCurWorkInPool();
		if (self->m_iCurThreadsInPool < self->m_iMaxThreadsInPool && self->m_iActThreadsInPool == self->m_iCurThreadsInPool)
		{
			StartNewThread();
		}
	}
}

inline UINT ThreadPool::IncCurThreadsInPool()
{
	return InterlockedIncrement(&m_iCurThreadsInPool);
}

inline UINT ThreadPool::DecCurThreadsInPool()
{
	return InterlockedDecrement(&m_iCurThreadsInPool);
}

inline UINT ThreadPool::IncCurWorkInPool()
{
	return InterlockedIncrement(&m_iCurWorkInPool);
}

inline UINT ThreadPool::DecCurWorkInPool()
{
	return InterlockedDecrement(&m_iCurWorkInPool);
}

inline UINT ThreadPool::IncActThreadsInPool()
{
	return InterlockedIncrement(&m_iActThreadsInPool);
}

inline UINT ThreadPool::DecActThreadsInPool()
{
	return InterlockedDecrement(&m_iActThreadsInPool);
}

inline DWORD ThreadPool::StartNewThread()
{
	DWORD threadid;
	HANDLE hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&ThreadPool::Queue, this, 0, &threadid);
	if (hThread == NULL)
		return 0;
	SetThreadPriority(hThread, THREAD_PRIORITY_LOWEST);
	IncCurThreadsInPool();
	return threadid;
}

ThreadPool::~ThreadPool()
{
	if (m_hIoCompPort)
	{
		for (UINT i = 0; i < m_iCurThreadsInPool; i++)
		{
			PostQueuedCompletionStatus(m_hIoCompPort, sizeof(INT), SHUTDOWN_IOCPTHREAD, NULL);
		}
		CloseHandle(m_hIoCompPort);
		m_hIoCompPort = NULL;
	}
}
