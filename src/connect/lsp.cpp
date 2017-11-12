#include "Environment.h"
#include "Debugger.h"
#include "SocketExtension.h"
#include "SupersocksRInteractive.h"
#include "SocketMappingPool.h"
#include "LayeredServiceProvider.h"
#include "NamespaceMappingTable.h"
#include "SocketMappingPool.h"
#include "LayeredServiceProvider.h"
#include "Socks5ProxyFilter.h"
#include "SocketEventController.h"

extern SupersocksRInteractive SupersocksRInteractive_Current;
extern LayeredServiceProvider LayeredServiceProvider_Current;
extern NamespaceMappingTable NamespaceMappingTable_Current;
extern SocketMappingPool SocketMappingPool_Current;
extern SocketEventController SocketEventController_Current;;
// TVFPTR_POINTER_LIST
LPFN_CONNECTEX Pfn_ConnectEx = NULL;
LPFN_WSASENDMSG Pfn_WSASendMsg = NULL;
CRITICAL_SECTION CS_FIONBIO;

void WSPClenupContext(SOCKET s)
{
	if (s != INVALID_SOCKET)
	{
		SocketBinderContext* context = SocketMappingPool_Current.Remove(s);
		if (context != NULL)
		{
			context->EnterLook();
			{
				context->PeerNameLen = 0;
				if (context->PeerName != NULL)
				{
					delete[] context->PeerName;
					context->PeerName = NULL;
				}
			}
			context->LeaveLook();
			context->Finalize();
			delete context;
		}
	}
}

int WSPAPI WSPShutdown(SOCKET s, int how, LPINT lpErrno)
{
	WSPClenupContext(s);
	return LayeredServiceProvider_Current.NextProcTable.lpWSPShutdown(s, how, lpErrno);
}

int WSPAPI WSPCloseSocket(SOCKET s, LPINT lpErrno)
{
	WSPClenupContext(s);
	return LayeredServiceProvider_Current.NextProcTable.lpWSPCloseSocket(s, lpErrno);
}

int WSPAPI WSPEventSelect(SOCKET s, WSAEVENT hEventObject, long lNetworkEvents, LPINT lpErrno)
{
	SupersocksRConfiguration* conf = SupersocksRInteractive_Current.Configuration();
	if (conf->EnableProxyClient)
	{
		SocketBinderContext* context = SocketMappingPool_Current.Get(s);
		if (context != NULL)
		{
			context->EnterLook();
			{
				SocketEventContext* evt = new SocketEventContext;
				evt->hEvent = hEventObject;
				evt->hWnd = NULL;
				evt->wMsg = 0;
				evt->lNetworkEvents = lNetworkEvents;

				context->Events.push_back(evt);
			}
			context->LeaveLook();
		}
	}
	return LayeredServiceProvider_Current.NextProcTable.lpWSPEventSelect(s, hEventObject, lNetworkEvents, lpErrno);
}

int Handshake(SOCKET s, const sockaddr * name, int namelen, SocketBinderContext* binder)
{
	TCHAR processname[MAX_PATH];
	GetModuleFileNameW(NULL, processname, MAX_PATH);

	char message[272];
	message[0] = 0x05;    // VER 
	message[1] = 0x01;    // NMETHODS
	message[2] = 0x00;    // METHODS 
	if (!SocketExtension::Send(s, message, 0, 3))
	{
		Debugger::Write(L"%s 发送第一次，错误 ...", processname);
		return ECONNREFUSED;
	}
	Debugger::Write("第一次成功发出的数据：%d, %d, %d", message[0], message[1], message[2]);
	if (!SocketExtension::Receive(s, message, 0, 2))
	{
		Debugger::Write("第一次错误收到的数据：%d, %d", message[0], message[1]);
		struct sockaddr_in name;
		int namelen = sizeof(struct sockaddr_in);
		getpeername(s, (struct sockaddr*)&name, &namelen);
		BYTE* num = (BYTE*)&name.sin_addr.s_addr;
		Debugger::Write(L"%s 第一次收到，错误 ... peername %d.%d.%d.%d:%d", processname, num[0], num[1], num[2], num[3], ntohs(name.sin_port));
		return ECONNABORTED;
	}
	if (message[1] != 0x00)
	{
		Debugger::Write(L"%s 被本地代理服务积极拒绝，错误 ...", processname);
		return ECONNABORTED;
	}
	Debugger::Write(L"%s --开始获取域名了哈？", processname);
	const struct sockaddr_in* sin = (struct sockaddr_in *)name;
	LPCSTR hostname = NamespaceMappingTable_Current.Get(sin->sin_addr.s_addr); // 逆向解析被污染以前的域名
	Debugger::Write(L"%s OK ----------%d 成功的获取到了域名？", processname, hostname != NULL);
	if (hostname != NULL && !Socks5ProxyFilter::Effective(hostname))
	{
		Debugger::Write(L"%s 这杯获取出一个域名，然而这是虚拟错误的，so ...", processname);
		return ECONNABORTED;
	}
	Debugger::Write(L"%s 开始握手 ...", processname);
	message[0] = 0x05; // VAR 
	message[1] = 0x01; // CMD 
	message[2] = 0x00; // RSV 
	message[3] = 0x00; // ATYPE 
	if (hostname == NULL)
	{
		message[3] = 0x01; // IPv4
		memcpy(&message[4], &sin->sin_addr.s_addr, 4); // ADDR
		memcpy(&message[8], &sin->sin_port, 2); // PORT
		if (!SocketExtension::Send(s, message, 0, 10))
		{
			return ECONNREFUSED;
		}
	}
	else
	{
		message[3] = 0x03; // hostname
		int offset = (int)strlen(hostname);
		if (offset <= 0)
		{
			return ECONNREFUSED;
		}
		message[4] = (char)offset;
		memcpy(&message[5], hostname, offset); // ADDR
		offset += 5;
		memcpy(&message[offset], &sin->sin_port, 2); // PORT
		offset += 2;
		if (!SocketExtension::Send(s, message, 0, offset))
		{
			return ECONNREFUSED;
		}
	}
	if (!SocketExtension::Receive(s, message, 0, 10))
	{
		return ECONNREFUSED;
	}
	if (message[1] != 0x00)
	{
		return ECONNREFUSED;
	}
	Debugger::Write(L"%s 成功鉴权 ...", processname);
	return NO_ERROR;
}


int WSPAPI WSPConnect(SOCKET s, const struct sockaddr* name, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS, LPINT lpErrno)
{
	int error = SOCKET_ERROR;
	do {
		if (!Socks5ProxyFilter::Effective((struct sockaddr_in *)name)) {
			WSPClenupContext(s); // 清理与此套接字连接绑定的的上下文
			error = LayeredServiceProvider_Current.NextProcTable.lpWSPConnect(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
			break;
		}
		TCHAR processname[MAX_PATH];
		GetModuleFileNameW(NULL, processname, MAX_PATH);
		struct sockaddr_in* sin = (struct sockaddr_in*)name;
		BYTE* p = (BYTE*)&sin->sin_addr.s_addr;
		Debugger::Write(L"%s ---lpWSPConnect peername %d.%d.%d.%d:%d", processname, p[0], p[1], p[2], p[3], ntohs(sin->sin_port));

		SocketBinderContext* binder = SocketMappingPool_Current.Get(s);
		if (binder == NULL) {
			Debugger::Write("无效不被代理认可的套接字");
			WSPShutdown(s, SD_BOTH, lpErrno);
			WSPCloseSocket(s, lpErrno);

			*lpErrno = WSAECONNREFUSED;
			error = ECONNRESET;
			break;
		}
		if (!SocketEventController_Current.SuspendEvent(s, binder)) {
			Debugger::Write("暂停事件失败");
			WSPShutdown(s, SD_BOTH, lpErrno);
			WSPCloseSocket(s, lpErrno);

			*lpErrno = WSAECONNREFUSED;
			error = ECONNRESET;
			break;
		}
		Debugger::Write("暂停事件成功");
		BOOL nonblock = binder->Nonblock;
		if (!SocketExtension::SetSocketNonblock(s, FALSE)) {
			Debugger::Write("设置阻塞失败");
			WSPShutdown(s, SD_BOTH, lpErrno);
			WSPCloseSocket(s, lpErrno);

			*lpErrno = WSAECONNREFUSED;
			error = ECONNRESET; // REAL-BLOCKING 
			break;
		}
		else {
			binder->Nonblock = nonblock;
		}
		// connect
		struct sockaddr_in server;
		memset(&server, 0, sizeof(struct sockaddr_in));
		server.sin_family = AF_INET;
		server.sin_port = htons(1080); // PORT
		server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		// 
		if (LayeredServiceProvider_Current.NextProcTable.lpWSPConnect(s, (struct sockaddr*)&server, sizeof(struct sockaddr_in),
			lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno) != NO_ERROR && *lpErrno != WSAEISCONN) // && *lpErrno != WSAEWOULDBLOCK) 
		{
			Debugger::Write(L"链接代理失败, LastError----%d", *lpErrno);
			WSPShutdown(s, SD_BOTH, lpErrno);
			WSPCloseSocket(s, lpErrno);

			*lpErrno = WSAECONNREFUSED;
			error = SOCKET_ERROR;
			break;
		}
	
		// handshake
		if (Handshake(s, name, namelen, binder) != NO_ERROR) {
			Debugger::Write(L"握手失败");
			WSPShutdown(s, SD_BOTH, lpErrno);
			WSPCloseSocket(s, lpErrno);

			*lpErrno = WSAECONNREFUSED;
			error = SOCKET_ERROR;
			break;
		}
		binder->AddressFrmily = name->sa_family;
		binder->PeerNameLen = namelen;
		binder->PeerName = new BYTE[namelen];
		memcpy(binder->PeerName, name, namelen);

		if (!SocketExtension::SetSocketNonblock(s, nonblock)) {
			Debugger::Write(L"设置异步失败");
			WSPShutdown(s, SD_BOTH, lpErrno);
			WSPCloseSocket(s, lpErrno);

			*lpErrno = WSAECONNREFUSED;
			error = ECONNRESET; // REAL-BLOCKING 
			break;
		}
		if (!SocketEventController_Current.ResumeEvent(s, binder)) {
			Debugger::Write(L"恢复事件失败");
			WSPShutdown(s, SD_BOTH, lpErrno);
			WSPCloseSocket(s, lpErrno);

			*lpErrno = WSAECONNREFUSED;
			error = ECONNRESET;
			break;
		}
		if (nonblock) {
			Debugger::Write(L"异步方式连接成功");
			SocketEventController_Current.SetEvent(s, binder, FD_CONNECT, 5);

			*lpErrno = WSAEWOULDBLOCK;
			error = SOCKET_ERROR;
			break;
		}
		else {
			Debugger::Write(L"同步方式连接成功");

			*lpErrno = NO_ERROR;
			error = NO_ERROR;
			break;
		}
	} while (0);
	return error;
}

int WSPAPI WSPIoctl(SOCKET s, DWORD dwIoControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer, LPVOID lpvOutBuffer, DWORD cbOutBuffer, LPDWORD lpcbBytesReturned,
	LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine, LPWSATHREADID lpThreadId, LPINT lpErrno)
{
	SupersocksRConfiguration* conf = SupersocksRInteractive_Current.Configuration();
	if (conf->EnableProxyClient && dwIoControlCode == FIONBIO)
	{
		int error = SOCKET_ERROR;
		error = LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer
			, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
		if (error == NO_ERROR)
		{
			SocketBinderContext* binder = SocketMappingPool_Current.Get(s);
			if (binder && lpvInBuffer) {
				binder->Nonblock = *(u_long*)lpvInBuffer;
			}
		}
		return error;
	}
	else
	{
		return LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer
			, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
	}
}

SOCKET WSPAPI WSPSocket(int af, int type, int protocol, LPWSAPROTOCOL_INFOW lpProtocolInfo, GROUP g, DWORD dwFlags, LPINT lpErrno)
{
	SOCKET s = LayeredServiceProvider_Current.NextProcTable.lpWSPSocket(af, type, protocol, lpProtocolInfo, g, dwFlags, lpErrno);
	SupersocksRConfiguration* conf = SupersocksRInteractive_Current.Configuration();
	if (conf->EnableProxyClient && s != INVALID_SOCKET)
	{
		if (af == AF_INET && (type == SOCK_STREAM || protocol == IPPROTO_TCP)) {
			SocketMappingPool_Current.Add(s);
		}
	}
	return s;
}

int WSPAPI WSPCleanup(LPINT lpErrno)
{
	return LayeredServiceProvider_Current.NextProcTable.lpWSPCleanup(lpErrno);
}

int WSPAPI WSPAsyncSelect(SOCKET s, HWND hWnd, unsigned int wMsg, long lEvent, LPINT lpErrno)
{
	SupersocksRConfiguration* conf = SupersocksRInteractive_Current.Configuration();
	if (conf->EnableProxyClient)
	{
		SocketBinderContext* context = SocketMappingPool_Current.Get(s);
		if (context != NULL)
		{
			context->EnterLook();
			{
				SocketEventContext* evt = new SocketEventContext;
				evt->hEvent = NULL;
				evt->hWnd = hWnd;
				evt->wMsg = wMsg;
				evt->lNetworkEvents = lEvent;

				context->Events.push_back(evt);
				context->HWndList.insert(evt->hWnd);
			}
			context->LeaveLook();
		}
	}
	return LayeredServiceProvider_Current.NextProcTable.lpWSPAsyncSelect(s, hWnd, wMsg, lEvent, lpErrno);
}

int WSPAPI WSPGetSockName(SOCKET s, sockaddr * name, LPINT namelen, LPINT lpErrno)
{
	if (name == NULL || namelen == NULL)
		return ERROR_NOACCESS;
	SocketBinderContext* binder = SocketMappingPool_Current.Get(s);
	if (binder == NULL)
		return LayeredServiceProvider_Current.NextProcTable.lpWSPGetSockName(s, name, namelen, lpErrno);
	struct sockaddr_in* sin = (struct sockaddr_in*)SocketExtension::LocalIP(FALSE);
	if (sin == NULL)
		return LayeredServiceProvider_Current.NextProcTable.lpWSPGetSockName(s, name, namelen, lpErrno);
	memcpy(name, sin, sizeof(struct sockaddr_in));
	if (lpErrno != NULL)
		*lpErrno = ERROR_SUCCESS;
	delete sin;
	return ERROR_SUCCESS;
}

int WSPAPI WSPGetPeerName(SOCKET s, sockaddr* name, LPINT namelen, LPINT lpErrno)
{
	if (name == NULL || namelen == NULL)
		return ERROR_NOACCESS;
	SocketBinderContext* binder = SocketMappingPool_Current.Get(s);
	if (binder == NULL)
		return LayeredServiceProvider_Current.NextProcTable.lpWSPGetPeerName(s, name, namelen, lpErrno);
	if (*namelen < binder->PeerNameLen)
		return ERROR_OUT_OF_STRUCTURES;
	memcpy(name, binder->PeerName, binder->PeerNameLen);
	if (lpErrno != NULL)
		*lpErrno = ERROR_SUCCESS;
	return ERROR_SUCCESS;
}

void WSPAPI StartProviderCompleted(WSPPROC_TABLE* sender, WSPPROC_TABLE* e)
{
	e->lpWSPConnect = &WSPConnect;
	e->lpWSPShutdown = &WSPShutdown;
	e->lpWSPCloseSocket = &WSPCloseSocket;
	e->lpWSPIoctl = &WSPIoctl;
	e->lpWSPSocket = &WSPSocket;
	e->lpWSPCleanup = &WSPCleanup;
	e->lpWSPEventSelect = &WSPEventSelect;
	e->lpWSPAsyncSelect = &WSPAsyncSelect;
	e->lpWSPGetPeerName = &WSPGetPeerName;
	e->lpWSPGetSockName = &WSPGetSockName;
};

int WSPAPI WSPStartup(WORD wversionrequested, LPWSPDATA lpwspdata, LPWSAPROTOCOL_INFOW lpProtoInfo, WSPUPCALLTABLE upcalltable, LPWSPPROC_TABLE lpproctable)
{
	TCHAR processname[MAX_PATH];
	GetModuleFileName(NULL, processname, MAX_PATH);
	Debugger::Write(L"%s Loading WSPStartup ...", processname);

	LayeredServiceProvider_Current.StartProviderCompleted = &StartProviderCompleted;
	return LayeredServiceProvider_Current.Start(MAKEWORD(2, 2), lpwspdata, lpProtoInfo, upcalltable, lpproctable);
};