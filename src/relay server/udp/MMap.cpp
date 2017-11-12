#include "MMap.h"

MMap::MMap()
{
	m_hMap = NULL;
	m_pLow = NULL;
	m_capacity = 0;
	m_hReadEvent = NULL;
	m_hWriteEvent = NULL;
}

MMap::~MMap()
{
	this->Close();
}

bool MMap::GetAvailable()
{
	return m_hMap != NULL && m_pLow != NULL;
}

void MMap::Close()
{
	if (m_pLow != NULL)
	{
		UnmapViewOfFile(m_pLow);
		m_pLow = NULL;
	}
	if (m_hMap != NULL)
	{
		CloseHandle(m_hMap);
		m_hMap = NULL;
	}
	m_capacity = 0;
}

void* MMap::GetFirstPtr()
{
	return m_pLow;
}

bool MMap::Bind(char * name, long long capacity)
{
	if (GetAvailable())
	{
		Close();
	}
	m_hMap = Create(name, capacity);
	if (m_hMap != NULL)
	{
		m_pLow = MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	}
	if (GetAvailable())
	{
		m_capacity = capacity;
		return true;
	}
	return false;
}

bool MMap::Connect(char* name, long long capacity)
{
	if (GetAvailable())
	{
		Close();
	}
	m_hMap = Open(name);
	if (m_hMap != NULL)
	{
		m_pLow = MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	}
	if (GetAvailable())
	{
		m_capacity = capacity;
		return true;
	}
	return false;
}

HANDLE MMap::Create(char* name, long long capacity)
{
	int dwMaximumSizeLow = (int)(capacity & ((INT64)(UINT64)-1));
	int dwMaximumSizeHigh = (int)(capacity >> 32);
	HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | 0, dwMaximumSizeHigh, dwMaximumSizeLow, name);
	if (hMap == NULL)
		hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | 0, dwMaximumSizeHigh, dwMaximumSizeLow, (LPCWSTR)name);
	return hMap;
}

HANDLE MMap::Open(char * name)
{
	HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
	if (hMap == NULL)
		hMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, (LPCWSTR)name);
	return hMap;
}