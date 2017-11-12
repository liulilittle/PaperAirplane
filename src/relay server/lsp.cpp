#include "Environment.h"
#include "Debugger.h"
#include "SocketExtension.h"
#include "PaperAirplaneInteractive.h"
#include "SocketMappingPool.h"
#include "LayeredServiceProvider.h"
#include "NamespaceMappingTable.h"
#include "SocketMappingPool.h"
#include "LayeredServiceProvider.h"
#include "SocketProxyChecker.h"
#include "Proxifier.h"

extern PaperAirplaneInteractive PaperAirplaneInteractive_Current;
extern LayeredServiceProvider LayeredServiceProvider_Current;
extern NamespaceMappingTable NamespaceMappingTable_Current;
extern SocketMappingPool SocketMappingPool_Current;

LPFN_WSASENDMSG PFN_WSASendMsg = NULL;
LPFN_CONNECTEX PFN_ConnectEx = NULL;

void CALLBACK PRXInteractiveTermination()
{
	Debugger::Write("PRXInteractiveTermination");
	PRXCloseAllTunnel(TRUE);
}

int WSPAPI WSPConnect(SOCKET s, const struct sockaddr* name, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS,
	LPQOS lpGQOS, LPINT lpErrno)
{
	int port = 0;
	if (SocketProxyChecker::Effective((struct sockaddr_in*)name) && (port = PRXCreateTunnel(s, name, namelen, SOCK_STREAM)) != 0)
	{
		struct sockaddr_in server;
		ZeroMemory(&server, sizeof(struct sockaddr_in));
		server.sin_family = AF_INET;
		server.sin_port = port; // PORT
		server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		return LayeredServiceProvider_Current.NextProcTable.lpWSPConnect(s, (struct sockaddr*)&server, sizeof(struct sockaddr_in),
			lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
	}
	return LayeredServiceProvider_Current.NextProcTable.lpWSPConnect(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
}

int WSPAPI WSPCloseSocket(SOCKET s, LPINT lpErrno)
{
	PRXCloseTunnel(s, TRUE); // ¹Ø±ÕËíµÀ
	return LayeredServiceProvider_Current.NextProcTable.lpWSPCloseSocket(s, lpErrno);
}

int WSPAPI WSPCleanup(LPINT lpErrno)
{
	int error = LayeredServiceProvider_Current.NextProcTable.lpWSPCleanup(lpErrno);
	if (error == NO_ERROR) {
		PRXCloseAllTunnel(TRUE);
	}
	return error;
}

BOOL PASCAL ConnectEx(SOCKET s, const sockaddr* name, int namelen, PVOID lpSendBuffer, DWORD dwSendDataLength, LPDWORD lpdwBytesSent,
	LPOVERLAPPED lpOverlapped)
{
	if (PFN_ConnectEx == NULL) {
		GUID metid = WSAID_CONNECTEX;
		PFN_ConnectEx = (LPFN_CONNECTEX)SocketExtension::GetExtensionFunction(s, metid);
	}
	if (PFN_ConnectEx == NULL) {
		return FALSE;
	}
	if (!SocketProxyChecker::Effective((struct sockaddr_in*)name))
		return PFN_ConnectEx(s, name, namelen, lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
	int port = 0;
	if (SocketProxyChecker::Effective((struct sockaddr_in*)name) && (port = PRXCreateTunnel(s, name, namelen, SOCK_STREAM)) != 0)
	{
		struct sockaddr_in server;
		ZeroMemory(&server, sizeof(struct sockaddr_in));
		server.sin_family = AF_INET;
		server.sin_port = port; // PORT
		server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		return PFN_ConnectEx(s, (struct sockaddr*)&server, sizeof(struct sockaddr_in), lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
	}
	return PFN_ConnectEx(s, name, namelen, lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
}

INT PASCAL SendMsgEx(SOCKET s, LPWSAMSG lpMsg, DWORD dwFlags, LPDWORD lpNumberOfBytesSent, LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	if (PFN_WSASendMsg == NULL) {
		GUID metid = WSAID_WSASENDMSG;
		PFN_WSASendMsg = (LPFN_WSASENDMSG)SocketExtension::GetExtensionFunction(s, metid);
	}
	if (PFN_WSASendMsg == NULL) {
		return SOCKET_ERROR;
	}
	if (SocketExtension::GetSocketType(s) == SOCK_DGRAM && SocketProxyChecker::Effective((struct sockaddr_in*)lpMsg->name)) {
		SocketBinderContext* context = NULL;
		WSABUF* buf = NULL;
		int len = lpMsg->dwBufferCount;
		if ((lpMsg->lpBuffers && len > 0) && (context = PRXCreateBinderContext(s))
			&& (buf = PRXAllocBufferContext(lpMsg->lpBuffers, len, lpMsg->name))) {

			struct sockaddr_in server;
			ZeroMemory(&server, sizeof(struct sockaddr_in));
			server.sin_family = AF_INET;
			server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			server.sin_port = context->BindPort;

			WSAMSG msg;
			msg.Control = lpMsg->Control;
			msg.dwFlags = lpMsg->dwFlags;

			msg.dwBufferCount = len;
			msg.lpBuffers = buf;

			msg.name = (struct sockaddr*)&server;
			msg.namelen = sizeof(struct sockaddr_in);

			int error = PFN_WSASendMsg(s, &msg, dwFlags, lpNumberOfBytesSent, lpOverlapped, lpCompletionRoutine);
			PRXFreeBufferContext(buf, len);
			return error;
		}
	}
	return PFN_WSASendMsg(s, lpMsg, dwFlags, lpNumberOfBytesSent, lpOverlapped, lpCompletionRoutine);
}

int WSPAPI WSPIoctl(SOCKET s, DWORD dwIoControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer, LPVOID lpvOutBuffer, DWORD cbOutBuffer,
	LPDWORD lpcbBytesReturned, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine, LPWSATHREADID lpThreadId, LPINT lpErrno)
{
	PaperAirplaneConfiguration* conf = PaperAirplaneInteractive_Current.Configuration();
	if (conf->EnableProxyClient && dwIoControlCode == SIO_GET_EXTENSION_FUNCTION_POINTER)
	{
		GUID connectex = WSAID_CONNECTEX;
		if (memcmp(lpvInBuffer, &connectex, sizeof(GUID)) == 0)
		{
			*((LPFN_CONNECTEX*)lpvOutBuffer) = &ConnectEx;
			*lpcbBytesReturned = sizeof(HANDLE);

			*lpErrno = NO_ERROR;
			return NO_ERROR;
		}
		GUID sendmsg = WSAID_WSASENDMSG;
		if (memcmp(lpvInBuffer, &sendmsg, sizeof(GUID)) == 0)
		{
			*((LPFN_WSASENDMSG*)lpvOutBuffer) = &SendMsgEx;
			*lpcbBytesReturned = sizeof(HANDLE);
			
			*lpErrno = NO_ERROR;
			return NO_ERROR;
		}
	}
	return LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer
		, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
}

int WSPAPI WSPSendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr FAR * lpTo,
	int iTolen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine, LPWSATHREADID lpThreadId, LPINT lpErrno)
{
	SocketBinderContext* context = NULL;
	WSABUF* buf = NULL;
	if ((lpBuffers && dwBufferCount > 0) && SocketProxyChecker::Effective((struct sockaddr_in*)lpTo) && (context = PRXCreateBinderContext(s))
		&& (buf = PRXAllocBufferContext(lpBuffers, dwBufferCount, lpTo))) {
		struct sockaddr_in server;
		ZeroMemory(&server, sizeof(struct sockaddr_in));
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		server.sin_port = context->BindPort;

		int error = LayeredServiceProvider_Current.NextProcTable.lpWSPSendTo(s, buf, dwBufferCount, lpNumberOfBytesSent, dwFlags, (struct sockaddr*)&server,
			sizeof(struct sockaddr_in), lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);

		PRXFreeBufferContext(buf, dwBufferCount);
		return error;
	}
	return LayeredServiceProvider_Current.NextProcTable.lpWSPSendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iTolen, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
}

void WSPAPI StartProviderCompleted(WSPPROC_TABLE* sender, WSPPROC_TABLE* e)
{
	e->lpWSPIoctl = &WSPIoctl;
	e->lpWSPConnect = &WSPConnect;
	e->lpWSPSendTo = &WSPSendTo;
	e->lpWSPCleanup = &WSPCleanup;
	e->lpWSPCloseSocket = &WSPCloseSocket;
}

int WSPAPI WSPStartup(WORD wversionrequested, LPWSPDATA lpwspdata, LPWSAPROTOCOL_INFOW lpProtoInfo, WSPUPCALLTABLE upcalltable, LPWSPPROC_TABLE lpproctable)
{
	TCHAR processname[MAX_PATH];
	GetModuleFileName(NULL, processname, MAX_PATH);
	Debugger::Write(L"%s Loading WSPStartup ...", processname);

	LayeredServiceProvider_Current.StartProviderCompleted = &StartProviderCompleted;
	PaperAirplaneInteractive_Current.Disconnected = &PRXInteractiveTermination;
	return LayeredServiceProvider_Current.Start(wversionrequested, lpwspdata, lpProtoInfo, upcalltable, lpproctable);
}