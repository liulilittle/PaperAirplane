#include "Environment.h"

__int64 GetSysTickCount64()
{
	LARGE_INTEGER freq = { 0 };
	LARGE_INTEGER tick;
	if (!freq.QuadPart)
	{
		QueryPerformanceFrequency(&freq);
	}
	QueryPerformanceCounter(&tick);
	__int64 seconds = tick.QuadPart / freq.QuadPart;
	__int64 leftpart = tick.QuadPart - (freq.QuadPart * seconds);
	__int64 millseconds = leftpart * 1000 / freq.QuadPart;
	__int64 retval = seconds * 1000 + millseconds;
	return retval;
}

int GetProcessorCoreCount()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors;
}
