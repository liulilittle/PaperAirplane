#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "Environment.h"

typedef void(WINAPI* PTHREADPOOLWAITCALLBACK)(void* state);
typedef PTHREADPOOLWAITCALLBACK LPTHREADPOOLWAITCALLBACK;

class ThreadPool
{
private:
	HANDLE m_hIoCompPort;
	UINT m_iCurThreadsInPool;
	UINT m_iCurWorkInPool;
	UINT m_iActThreadsInPool;

	UINT m_iMaxThreadsInPool;
	UINT M_iMinThreadsInPool;

	static DWORD WINAPI Queue(ThreadPool* self);

	UINT IncCurThreadsInPool();
	UINT DecCurThreadsInPool();
	UINT IncCurWorkInPool();
	UINT DecCurWorkInPool();
	UINT IncActThreadsInPool();
	UINT DecActThreadsInPool();

	DWORD StartNewThread();

public:
	ThreadPool(int iMinConcurrency, int iMaxConcurrency);
	~ThreadPool();

	void QueueUserWork(LPTHREADPOOLWAITCALLBACK callback, void* state);
};
#endif
