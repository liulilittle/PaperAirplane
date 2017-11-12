#include "Environment.h"
#include "Debugger.h"
#include "SocketExtension.h"
#include "PaperAirplaneInteractive.h"
#include "SocketMappingPool.h"
#include "LayeredServiceProvider.h"
#include "NamespaceMappingTable.h"
#include "SocketMappingPool.h"
#include "LayeredServiceProvider.h"
#include "Socks5ProxyFilter.h"
#include "SocketEventController.h"
#include "Proxifier.h"

extern PaperAirplaneInteractive PaperAirplaneInteractive_Current;
extern LayeredServiceProvider LayeredServiceProvider_Current;
extern NamespaceMappingTable NamespaceMappingTable_Current;
extern SocketMappingPool SocketMappingPool_Current;
extern SocketEventController SocketEventController_Current;

LPFN_CONNECTEX PFN_ConnectEx;

void CALLBACK PRXInteractiveTermination()
{
	Debugger::Write("PRXInteractiveTermination");
	PRXCloseAllTunnel(TRUE);
}

int WSPAPI WSPConnect(SOCKET s, const struct sockaddr* name, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS,
	LPQOS lpGQOS, LPINT lpErrno)
{
	int port = 0;
	if (Socks5ProxyFilter::Effective(name) && (port = PRXCreateTunnel(s, name, namelen, SOCK_STREAM)) != 0)
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
	PRXCloseTunnel(s, TRUE); // 关闭隧道
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
	if (PFN_ConnectEx == NULL)
		PFN_ConnectEx = (LPFN_CONNECTEX)SocketExtension::GetExtensionFunction(s, WSAID_CONNECTEX);
	if (PFN_ConnectEx == NULL)
		return FALSE;
	if (!Socks5ProxyFilter::Effective((struct sockaddr*)name))
		return PFN_ConnectEx(s, name, namelen, lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
	int port = 0;
	if (Socks5ProxyFilter::Effective((struct sockaddr*)name) && (port = PRXCreateTunnel(s, name, namelen, SOCK_STREAM)) != 0)
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
	}
	return LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer
		, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
}

int WSPAPI WSPSendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr FAR * lpTo,
	int iTolen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine, LPWSATHREADID lpThreadId, LPINT lpErrno)
{
	PaperAirplaneConfiguration* conf = PaperAirplaneInteractive_Current.Configuration();
	SocketBinderContext* context = NULL;
	Debugger::Write("WSPSendTo asdmioasnd ioasndoasndioa");
	if (lpBuffers && dwBufferCount > 0 && Socks5ProxyFilter::Effective(lpTo) && (context = PRXCreateBinderContext(s))) {
		HANDLE heap = GetProcessHeap();
		WSABUF* buf = (WSABUF*)HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(WSABUF) * dwBufferCount);
		Debugger::Write("分配的内存是无效的 %d", buf == NULL);
		WSABUF* deposit[2] = { buf, lpBuffers };
		for (size_t i = 0; i < dwBufferCount; i++) {
			buf->len = lpBuffers->len;
			buf->buf = (CHAR*)HeapAlloc(heap, HEAP_ZERO_MEMORY, buf->len + 272);
			int len;
			Debugger::Write("Begin PRXAddressToEndPoint");
			PRXAddressToEndPoint(lpTo, (BYTE*)buf->buf, PAPERAIRPLANE_PROTOCOLTYPE_UDP, len);

			Debugger::Write("End PRXAddressToEndPoint");
			if (buf->len > 0) {
				buf->len += len;
				Debugger::Write("buf.buf is NULL %d, lpBuffers->buf is NULL %d, buf.len %d, lpBuffers->len %d, len---------%d", buf->buf == NULL,
					lpBuffers->buf == NULL, buf->len, lpBuffers->len, len);

				memcpy(&buf->buf[len], lpBuffers->buf, lpBuffers->len);
			}
			Debugger::Write("set server addr");
			buf++;
			lpBuffers++;
		}
		lpBuffers = deposit[0];
		buf = deposit[0];

		struct sockaddr_in server;
		ZeroMemory(&server, sizeof(struct sockaddr_in));
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		server.sin_port = context->BindPort;

		Debugger::Write("Begin LayeredServiceProvider_Current.NextProcTable.lpWSPSendTo");

		int error = LayeredServiceProvider_Current.NextProcTable.lpWSPSendTo(s, buf, dwBufferCount, lpNumberOfBytesSent, dwFlags, (struct sockaddr*)&server,
			sizeof(struct sockaddr_in), lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
		for (size_t i = 0; i < dwBufferCount; i++) {
			HeapFree(heap, HEAP_NO_SERIALIZE, buf[i].buf);
		}
		HeapFree(heap, HEAP_NO_SERIALIZE, buf);
		return error;
	}
	return LayeredServiceProvider_Current.NextProcTable.lpWSPSendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iTolen, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
}

void WSPAPI StartProviderCompleted(WSPPROC_TABLE* sender, WSPPROC_TABLE* e)
{
	e->lpWSPIoctl = &WSPIoctl;
	e->lpWSPConnect = &WSPConnect;
}

int WSPAPI WSPStartup(WORD wversionrequested, LPWSPDATA lpwspdata, LPWSAPROTOCOL_INFOW lpProtoInfo, WSPUPCALLTABLE upcalltable, LPWSPPROC_TABLE lpproctable)
{
	TCHAR processname[MAX_PATH];
	GetModuleFileName(NULL, processname, MAX_PATH);
	Debugger::Write(L"%s Loading WSPStartup ...", processname);

	LayeredServiceProvider_Current.StartProviderCompleted = &StartProviderCompleted;
	PaperAirplaneInteractive_Current.Disconnected = &PRXInteractiveTermination;
	return LayeredServiceProvider_Current.Start(MAKEWORD(2, 2), lpwspdata, lpProtoInfo, upcalltable, lpproctable);
}