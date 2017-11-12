#ifndef NAMESPACEMAPPINGTABLE_H
#define NAMESPACEMAPPINGTABLE_H

#include "Environment.h"

class NamespaceMappingTable
{
private:
	hash_map<ULONG, LPCSTR>* m_hosts;
	CRITICAL_SECTION m_cs;
	hash_map<string, ULONG>* m_addrs;

	void Enter();

	void Leave();

public:
	NamespaceMappingTable();

	~NamespaceMappingTable();

	void Add(ULONG addr, LPCSTR hostname);

	void Add(string hostname, ULONG addr);

	LPCSTR Get(ULONG addr);

	ULONG Get(string hostname);

	void Remove(ULONG addr);

	void RemoveAll();

	bool ContainsKey(ULONG s);

	bool ContainsKey(string hostname);
};
#endif