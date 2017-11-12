#include "SocketMappingPool.h"

SocketMappingPool SocketMappingPool_Current;

inline void SocketMappingPool::Enter()
{
	EnterCriticalSection(&m_cs);
}

inline void SocketMappingPool::Leave()
{
	LeaveCriticalSection(&m_cs);
}

SocketMappingPool::SocketMappingPool()
{
	m_mapTable = new hash_map<SOCKET, SocketBinderContext*>;
	memset(&m_cs, 0, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&m_cs);
}

SocketMappingPool::~SocketMappingPool()
{
	this->Enter();
	if (m_mapTable != NULL)
	{
		this->RemoveAll();
		delete m_mapTable;
		m_mapTable = NULL;
	}
	this->Leave();
	DeleteCriticalSection(&m_cs);
}

SocketBinderContext * SocketMappingPool::Add(SOCKET s)
{
	SocketBinderContext* context = NULL;
	this->Enter();
	if (m_mapTable != NULL && !this->ContainsKey(s))
	{
		context = new SocketBinderContext();
		context->ctor();
		context->SocketHandle = s;
		this->Add(context);
	}
	this->Leave();
	return context;
}

SocketBinderContext* SocketMappingPool::Add(SocketBinderContext * context)
{
	if (context != NULL)
	{
		this->Enter();
		m_mapTable->insert(hash_map<SOCKET, SocketBinderContext*>::value_type(context->SocketHandle, context));
		this->Leave();
	}
	return context;
}

SocketBinderContext* SocketMappingPool::Get(SOCKET s)
{
	SocketBinderContext* context = NULL;
	this->Enter();
	do
	{
		if (m_mapTable == NULL || m_mapTable->empty())
			break;
		context = (*m_mapTable)[s];
	} while (0);
	this->Leave();
	return context;
}

SocketBinderContext* SocketMappingPool::Remove(SOCKET s)
{
	SocketBinderContext* context = NULL;
	this->Enter();
	do 
	{
		if (m_mapTable == NULL || m_mapTable->empty())
			break;
		context = (*m_mapTable)[s];
		m_mapTable->erase(s);
	} while (0);
	this->Leave();
	return context;
}

SocketBinderContext * SocketMappingPool::Remove(SocketBinderContext * context)
{
	if (context != NULL)
	{
		this->Remove(context->SocketHandle);
	}
	return context;
}

void SocketMappingPool::RemoveAll()
{
	this->Enter();
	if (m_mapTable != NULL && !m_mapTable->empty())
	{
		m_mapTable->clear();
	}
	this->Leave();
}

bool SocketMappingPool::ContainsKey(SOCKET s)
{
	bool contains = false;
	this->Enter();
	do
	{
		if (m_mapTable == NULL || m_mapTable->empty())
			break;
		else
		{
			hash_map<SOCKET, SocketBinderContext*>::iterator i = m_mapTable->find(s);
			contains = (i != m_mapTable->end());
		}
	} while (0);
	this->Leave();
	return contains;
}

void SocketBinderContext::ctor()
{
	this->SocketHandle = INVALID_SOCKET;
	this->PeerName = NULL;
	this->PeerNameLen = 0;
	this->Nonblock = FALSE;
	memset(&this->Look, 0, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&this->Look);
}