#ifndef MMAP_H
#define MMAP_H

#include "Environment.h"

class MMap
{
private:
	HANDLE m_hMap;
	PVOID m_pLow;
	UINT64 m_capacity;
	HANDLE m_hReadEvent;
	HANDLE m_hWriteEvent;

public:
	MMap();
	~MMap();

	bool GetAvailable();
	void Close();
	void* GetFirstPtr();
	bool Connect(char* name, long long capacity);
	bool Bind(char* name, long long capacity);

private:
	HANDLE Create(char* name, long long capacity);
	HANDLE Open(char* name);
};
#endif

