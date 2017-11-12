#include "SocketExtension.h"
#include "LayeredServiceProvider.h"

extern LayeredServiceProvider LayeredServiceProvider_Current;

VOID * SocketExtension::GetExtensionFunction(SOCKET s, GUID * clasid)
{
	if (clasid == NULL || s == INVALID_SOCKET)
	{
		return NULL;
	}
	VOID* func = { 0 };
	DWORD size = 0;
	//
	WSATHREADID threadid;
	threadid.ThreadHandle = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
	threadid.Reserved = NULL;
	INT error = 0;
	if (LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, clasid,
		sizeof(GUID), &func, sizeof(VOID*), &size, NULL, NULL, &threadid, &error) == SOCKET_ERROR)
	{
		CloseHandle(threadid.ThreadHandle);
		return NULL;
	}
	CloseHandle(threadid.ThreadHandle);
	return func;
}

VOID * SocketExtension::GetExtensionFunction(GUID * clasid)
{
	if (clasid == NULL)
	{
		return NULL;
	}
	INT error = 0;
	SOCKET s = LayeredServiceProvider_Current.NextProcTable.lpWSPSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED, &error);
	if (s == INVALID_SOCKET)
	{
		return NULL;
	}
	VOID* func = GetExtensionFunction(s, clasid);
	LayeredServiceProvider_Current.NextProcTable.lpWSPShutdown(s, SD_BOTH, &error);
	LayeredServiceProvider_Current.NextProcTable.lpWSPCloseSocket(s, &error);
	return func;
}

VOID * SocketExtension::GetExtensionFunction(SOCKET s, GUID clasid)
{
	return GetExtensionFunction(s, &clasid);
}

VOID * SocketExtension::GetExtensionFunction(GUID clasid)
{
	return GetExtensionFunction(&clasid);
}

bool SocketExtension::SetSocketNonblock(SOCKET s, u_long nonblock)
{
	WSATHREADID id;
	id.ThreadHandle = GetCurrentThread();
	id.Reserved = NULL;
	INT err;
	DWORD pvout, pvlen = sizeof(DWORD);
	return LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, FIONBIO, &nonblock, sizeof(u_long), &pvout,
		sizeof(DWORD), &pvlen, NULL, NULL, &id, &err) == 0;
}

bool SocketExtension::Receive(SOCKET s, char * buffer, int ofs, int len)
{
	char* scan = &buffer[ofs];
	INT i = 0;
	while (i < len)
	{
		DWORD size = recv(s, scan, (len - i), 0);
		if (size <= 0)
			return false;
		i += size;
		scan += size;
	}
	return true;
}

int SocketExtension::ReveiveForm(SOCKET s, char * buf, int ofs, int len, sockaddr_in * addr)
{
	int size = sizeof(struct sockaddr_in);
	return recvfrom(s, &buf[ofs], len, 0, (struct sockaddr*)addr, &size);
}

int SocketExtension::SendTo(SOCKET s, char * buf, int ofs, int len, sockaddr_in * addr)
{
	return sendto(s, &buf[ofs], len, 0, (struct sockaddr*)addr, sizeof(struct sockaddr_in));
}

bool SocketExtension::Send(SOCKET s, char * buffer, int ofs, int len)
{
	return send(s, &buffer[ofs], len, 0) != SOCKET_ERROR;
}

struct sockaddr* SocketExtension::LocalIP(BOOL localhost)
{
	if (localhost)
	{
		char hostname[MAX_PATH];
		if (gethostname(hostname, MAX_PATH) != 0)
			return NULL;
		hostent* hostinfo = gethostbyname(hostname);
		if (hostinfo == NULL || *hostinfo->h_addr_list == NULL)
			return NULL;
		if (hostinfo->h_addrtype == AF_INET)
		{
			struct sockaddr_in* addr = new struct sockaddr_in;
			addr->sin_family = AF_INET;
			addr->sin_port = 0;
			addr->sin_addr.s_addr = *(u_long*)*hostinfo->h_addr_list;
			return (struct sockaddr*)addr;
		}
		else if (hostinfo->h_addrtype != AF_INET6)
			return NULL;
		else
		{
			struct sockaddr_in6* addr = new struct sockaddr_in6;
			memset(addr, 0, sizeof(struct sockaddr_in6));
			addr->sin6_family = AF_INET6;
			addr->sin6_port = 0;
			memcpy(addr->sin6_addr.u.Byte, *hostinfo->h_addr_list, 16);
			return (struct sockaddr*)addr;
		}
	}
	else
	{
		vector<ULONG>* localEPs = SocketExtension::LocalIPs();
		if (localEPs == NULL)
			return NULL;
		struct sockaddr_in* addr = NULL;
		if (!localEPs->empty())
		{
			addr = new struct sockaddr_in;
			addr->sin_family = AF_INET;
			addr->sin_port = 0;
			addr->sin_addr.s_addr = (*localEPs)[0];
		}
		delete localEPs;
		return (struct sockaddr*)addr;
	}
}

vector<ULONG>* SocketExtension::LocalIPs()
{
	PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
	unsigned long stSize = sizeof(IP_ADAPTER_INFO);
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	if (ERROR_BUFFER_OVERFLOW == nRel)
	{
		delete pIpAdapterInfo;
		pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
		nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	}
	vector<ULONG>* address = NULL;
	if (ERROR_SUCCESS == nRel)
	{
		while (pIpAdapterInfo)
		{
			IP_ADDR_STRING *pIpAddrString = &(pIpAdapterInfo->IpAddressList);
			do
			{
				ULONG addrnum = inet_addr(pIpAddrString->IpAddress.String);
				if (addrnum != 0)
				{
					if (address == NULL)
					{
						address = new vector<ULONG>;
					}
					address->push_back(addrnum);
				}
				pIpAddrString = pIpAddrString->Next;
			} while (pIpAddrString);
			pIpAdapterInfo = pIpAdapterInfo->Next;
		}
	}
	if (pIpAdapterInfo)
		delete pIpAdapterInfo;
	return address;
}

void SocketExtension::CloseSocket(SOCKET s)
{
	if (s != INVALID_SOCKET)
	{
		INT error;
		LayeredServiceProvider_Current.NextProcTable.lpWSPShutdown(s, SD_BOTH, &error);
		LayeredServiceProvider_Current.NextProcTable.lpWSPCloseSocket(s, &error);
	}
}

int SocketExtension::GetSocketType(SOCKET s)
{
	if (s != INVALID_SOCKET) {
		int sockType = 0;
		int optlen = 4;
		if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&sockType, &optlen) != SOCKET_ERROR) {
			return sockType;
		}
	}
	return SOCKET_ERROR;
}

int* SocketExtension::IOControl(SOCKET s, int ioControlCode, void* optionInValue, int optionInValueLen, void* optionOutValue, int optionOutValueLen)
{
	WSATHREADID threadid;
	threadid.ThreadHandle = GetCurrentThread();
	threadid.Reserved = NULL;
	int* result = new int(0);
	INT err = 0;
	if (LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, ioControlCode, optionInValue, optionInValueLen,
		optionOutValue, optionOutValueLen, (LPDWORD)&result, NULL, NULL, &threadid, &err) != 0)
	{
		delete result;
		return NULL;
	}
	return result;
}