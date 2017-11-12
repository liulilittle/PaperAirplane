#ifndef SOCKETEXTENSION_H
#define SOCKETEXTENSION_H

#include "Environment.h"

class SocketExtension
{
public:
	static VOID* GetExtensionFunction(SOCKET s, GUID* clasid);

	static VOID* GetExtensionFunction(GUID* clasid);

	static VOID* GetExtensionFunction(SOCKET s, GUID clasid);

	static VOID* GetExtensionFunction(GUID clasid);

	static bool SetSocketNonblock(SOCKET s, u_long nonblock);

public:
	static bool Receive(SOCKET s, char* buffer, int ofs, int len);

	static int ReveiveForm(SOCKET s, char* buf, int ofs, int len, struct sockaddr_in* addr);

	static int SendTo(SOCKET s, char* buf, int ofs, int len, struct sockaddr_in* addr);

	static bool Send(SOCKET s, char* buffer, int ofs, int len);

	static struct sockaddr* LocalIP(BOOL localhost);

	static vector<ULONG>* LocalIPs();

	static void CloseSocket(SOCKET s);

	static int* IOControl(SOCKET s, int ioControlCode, void* optionInValue, int optionInValueLen, void* optionOutValue, int optionOutValueLen);
};
#endif
