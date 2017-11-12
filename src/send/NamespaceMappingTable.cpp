#include "NamespaceMappingTable.h"

NamespaceMappingTable NamespaceMappingTable_Current;

void NamespaceMappingTable::Enter()
{
	EnterCriticalSection(&m_cs);
}

void NamespaceMappingTable::Leave()
{
	LeaveCriticalSection(&m_cs);
}

NamespaceMappingTable::NamespaceMappingTable()
{
	m_hosts = new hash_map<ULONG, LPCSTR>;
	m_addrs = new hash_map<string, ULONG>;
	memset(&m_cs, 0, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&m_cs);
}

NamespaceMappingTable::~NamespaceMappingTable()
{
	this->Enter();
	{
		if (m_hosts != NULL)
		{
			this->RemoveAll();
			delete m_hosts;
			m_hosts = NULL;
		}
		if (m_addrs != NULL)
		{
			delete m_addrs;
			m_addrs = NULL;
		}
	}
	this->Leave();
	DeleteCriticalSection(&m_cs);
};

void NamespaceMappingTable::Add(ULONG addr, LPCSTR hostname)
{
	this->Enter();
	{
		if (m_hosts != NULL)
		{
			m_hosts->insert(hash_map<ULONG, LPCSTR>::value_type(addr, hostname));
		}
	}
	this->Leave();
}

void NamespaceMappingTable::Add(string hostname, ULONG addr)
{
	this->Enter();
	{
		if (m_addrs != NULL)
		{
			m_addrs->insert(hash_map<string, ULONG>::value_type(hostname, addr));
		}
	}
	this->Leave();
}

LPCSTR NamespaceMappingTable::Get(ULONG addr)
{
	LPCSTR hostname = NULL;
	this->Enter();
	do
	{
		if (m_hosts == NULL || m_hosts->empty())
			break;
		else
		{
			hostname = (*m_hosts)[addr];
			if (hostname != NULL && IsBadReadPtr(hostname, 1))
			{
				hostname = NULL;
				break;
			}
		}
	} while (0);
	this->Leave();
	return hostname;
}

ULONG NamespaceMappingTable::Get(string hostname)
{
	ULONG addr = 0;
	this->Enter();
	do
	{
		if (m_addrs == NULL || m_addrs->empty())
			break;
		else
			addr = (*m_addrs)[hostname];
	} while (0);
	this->Leave();
	return addr;
}

void NamespaceMappingTable::Remove(ULONG addr)
{
	this->Enter();
	{
		if (m_hosts != NULL && !m_hosts->empty())
		{
			m_hosts->erase(addr);
		}
	}
	this->Leave();
}

void NamespaceMappingTable::RemoveAll()
{
	this->Enter();
	{
		if (m_hosts != NULL && !m_hosts->empty())
		{
			m_hosts->clear();
		}
	}
	this->Leave();
}

bool NamespaceMappingTable::ContainsKey(ULONG s)
{
	bool contains = false;
	this->Enter();
	do
	{
		if (m_hosts == NULL || m_hosts->empty())
			break;
		else
		{
			hash_map<ULONG, LPCSTR>::iterator i = m_hosts->find(s);
			contains = (i != m_hosts->end());
		}
	} while (0);
	this->Leave();
	return contains;
}

bool NamespaceMappingTable::ContainsKey(string hostname)
{
	bool contains = false;
	this->Enter();
	do
	{
		if (m_addrs == NULL || m_addrs->empty())
			break;
		else
		{
			hash_map<string, ULONG>::iterator i = m_addrs->find(hostname);
			contains = (i != m_addrs->end());
		}
	} while (0);
	this->Leave();
	return contains;
}
