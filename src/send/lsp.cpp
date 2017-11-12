#include "Environment.h"
#include "SocketExtension.h"
#include "SupersocksRInteractive.h"
#include "SocketMappingPool.h"
#include "LayeredServiceProvider.h"
#include "NamespaceMappingTable.h"
#include "SocketMappingPool.h"
#include "LayeredServiceProvider.h"
#include "Socks5ProxyFilter.h"

extern SupersocksRInteractive SupersocksRInteractive_Current;
extern LayeredServiceProvider LayeredServiceProvider_Current;
extern NamespaceMappingTable NamespaceMappingTable_Current;
extern SocketMappingPool SocketMappingPool_Current;
// TVFPTR_POINTER_LIST
LPFN_CONNECTEX Pfn_ConnectEx = NULL;
LPFN_WSASENDMSG Pfn_WSASendMsg = NULL;

void WSPClenupContext(SOCKET s)
{
	if (s != INVALID_SOCKET)
	{
		SocketBinderContext* context = SocketMappingPool_Current.Remove(s);
		if (context != NULL)
		{
			context->PeerNameLen = 0;
			if (context->PeerName != NULL)
			{
				delete[] context->PeerName;
				context->PeerName = NULL;
			}
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

int WSPAPI WSPEventSelect(
	__in SOCKET s,
	__in WSAEVENT hEventObject,
	__in long lNetworkEvents,
	__out LPINT lpErrno
)
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
				evt->hEvent = (__int64)hEventObject;
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

int WSPPauseEvent(SOCKET s, SocketBinderContext* binder)
{
	if (binder == NULL || s == INVALID_SOCKET)
	{
		return SOCKET_ERROR;
	}
	int err = 0; // THE CLEANUP BIND EVENT OBJECT
	if (LayeredServiceProvider_Current.NextProcTable.lpWSPEventSelect(s, 0, NULL, &err))
	{
		return ECONNRESET;
	}
	binder->EnterLook();
	{
		for each(HWND hWnd in binder->HWndList)
		{
			err = 0;
			if (LayeredServiceProvider_Current.NextProcTable.lpWSPAsyncSelect(s, hWnd, 0, 0, &err))
			{
				binder->LeaveLook();
				return ECONNRESET; // WSAAsyncSelect(s, hWnd, 0, 0);
			}
		}
	}
	binder->LeaveLook();
	return 0;
}

int WSPResumeEvent(SOCKET s, SocketBinderContext* binder)
{
	if (binder == NULL || s == INVALID_SOCKET)
	{
		return SOCKET_ERROR;
	}
	binder->EnterLook();
	{
		vector<SocketEventContext*>* events = &binder->Events;
		for (size_t i = 0, len = events->size(); i < len; i++)
		{
			SocketEventContext* evt = (*events)[i];
			int err = 0;
			if (evt->hWnd == NULL)
			{
				if (LayeredServiceProvider_Current.NextProcTable.lpWSPEventSelect(s, (HANDLE)evt->hEvent, evt->lNetworkEvents, &err))
				{
					return ECONNRESET;
				}
			}
			else
			{
				if (LayeredServiceProvider_Current.NextProcTable.lpWSPAsyncSelect(s, evt->hWnd, evt->wMsg, (long)evt->hEvent, &err))
				{
					return ECONNRESET;
				}
			}
			delete evt;
		}
		events->clear();
	}
	binder->LeaveLook();
	return 0;
}

int Handshake(SOCKET s, const sockaddr * name, int namelen, SocketBinderContext* binder)
{
	TCHAR processname[MAX_PATH];
	GetModuleFileName(NULL, processname, MAX_PATH);

	if (WSPPauseEvent(s, binder) != 0)
	{
		Debugger::Write(L"%s 暂停套接字事件失败 ...", processname);
		return ECONNRESET;
	}
	Debugger::Write(L"%s 暂停套接字事件成功 ...", processname);

	BOOL nonblock = binder->Nonblock;
	if (nonblock && !SocketExtension::SetSocketNonblock(s, FALSE))
		return ECONNRESET; // REAL-BLOCKING 
	else
		binder->Nonblock = nonblock;
	Debugger::Write(L"%s 设置阻塞模式成功 ...", processname);

	char message[272];
	message[0] = 0x05;    // VER 
	message[1] = 0x01;    // NMETHODS
	message[2] = 0x00;    // METHODS 
	if (!SocketExtension::Send(s, message, 0, 3))
	{
		Debugger::Write(L"%s 发送第一次，错误 ...", processname);
		return ECONNREFUSED;
	}
	if (!SocketExtension::Receive(s, message, 0, 2))
	{
		Debugger::Write(L"%s 第一次收到，错误 ...", processname);
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
	BYTE* remoteaddr = (BYTE*)&sin->sin_addr.s_addr;
	struct sockaddr_in proxyin4;
	INT err;
	INT proxyaddrlen = sizeof(struct sockaddr_in);
	LayeredServiceProvider_Current.NextProcTable.lpWSPGetPeerName(s, (struct sockaddr*)&proxyin4, &proxyaddrlen, &err);
	BYTE* proxyaddr = (BYTE*)&proxyin4.sin_addr.s_addr;

	Debugger::Write(L"%s 开始握手 ...host---- %d.%d.%d.%d:%d :: proxy--- %d.%d.%d.%d:%d", processname, 
		remoteaddr[0], remoteaddr[1], remoteaddr[2], remoteaddr[3], ntohs(sin->sin_port),
		proxyaddr[0], proxyaddr[1], proxyaddr[2], proxyaddr[3], ntohs(proxyin4.sin_port)
	);

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
			return ECONNREFUSED;
	}
	else
	{
		message[3] = 0x03; // hostname
		int offset = (int)strlen(hostname);
		if (offset <= 0)
			return ECONNREFUSED;
		message[4] = (char)offset;
		memcpy(&message[5], hostname, offset); // ADDR
		offset += 5;
		memcpy(&message[offset], &sin->sin_port, 2); // PORT
		offset += 2;
		if (!SocketExtension::Send(s, message, 0, offset))
			return ECONNREFUSED;
	}
	if (!SocketExtension::Receive(s, message, 0, 10))
		return ECONNREFUSED;
	if (message[1] != 0x00)
		return ECONNREFUSED;
	Debugger::Write(L"%s 成功鉴权 ...", processname);
	if (WSPResumeEvent(s, binder) != 0)
	{
		Debugger::Write(L"%s 无法恢复事件 ...", processname);
		return ECONNRESET;
	}

	if (nonblock && !SocketExtension::SetSocketNonblock(s, TRUE))
	{
		Debugger::Write(L"%s 无法设置成异步 ...", processname);
		return ECONNABORTED;
	}
	Debugger::Write(L"%s 成功的完成握手 ...", processname);

	return 0;
}

int Handshake(SOCKET s, INT* lpErrno)
{
	SupersocksRConfiguration* conf = SupersocksRInteractive_Current.Configuration();
	SocketBinderContext* binder = NULL;
	int error = NO_ERROR;
	if (conf->EnableProxyClient && (binder = SocketMappingPool_Current.Get(s)))
	{
		binder->EnterLook();
		if (binder->RequireHandshake)
		{
			binder->RequireHandshake = FALSE;
			error = Handshake(s, (sockaddr*)binder->PeerName, binder->PeerNameLen, binder);
			if (error != NO_ERROR)
			{
				*lpErrno = error;
				error = SOCKET_ERROR;

				WSPShutdown(s, SD_BOTH, lpErrno);
				WSPCloseSocket(s, lpErrno);
			}
		}
		binder->LeaveLook();
	}
	return error;
}

//WSPConnect
int WSPAPI WSPConnect(
	SOCKET s,
	const struct sockaddr* name,
	int namelen,
	LPWSABUF lpCallerData,
	LPWSABUF lpCalleeData,
	LPQOS lpSQOS,
	LPQOS lpGQOS,
	LPINT lpErrno)
{
	TCHAR processname[MAX_PATH];
	GetModuleFileName(NULL, processname, MAX_PATH);
	Debugger::Write(L"%s WSPConnect ...", processname);

	if (s == INVALID_SOCKET || !Socks5ProxyFilter::Effective((struct sockaddr_in *)name))
	{
		WSPClenupContext(s); // 清理与此套接字连接绑定的的上下文
		return LayeredServiceProvider_Current.NextProcTable.lpWSPConnect(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
	}
	struct sockaddr_in server;
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(1080); // PORT
	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	int error = LayeredServiceProvider_Current.NextProcTable.lpWSPConnect(s, (struct sockaddr*)&server, sizeof(struct sockaddr_in),
		lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
	SocketBinderContext* binder = SocketMappingPool_Current.Get(s);
	if (binder != NULL)
	{
		binder->EnterLook();
		{
			binder->AddressFrmily = name->sa_family;
			binder->PeerNameLen = namelen;
			binder->PeerName = new BYTE[namelen];
			memcpy(binder->PeerName, name, namelen);
		}
		binder->LeaveLook();
	}
	return error;
}

BOOL PASCAL ConnectEx(SOCKET s, const sockaddr* name, int namelen, PVOID lpSendBuffer, DWORD dwSendDataLength, LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped)
{
	TCHAR processname[MAX_PATH];
	GetModuleFileName(NULL, processname, MAX_PATH);
	Debugger::Write(L"%s ConnectEx ...", processname);

	if (Pfn_ConnectEx == NULL)
		Pfn_ConnectEx = (LPFN_CONNECTEX)SocketExtension::GetExtensionFunction(s, WSAID_CONNECTEX);;
	if (Pfn_ConnectEx == NULL)
	{
		WSPClenupContext(s); // 清理与此套接字连接绑定的的上下文
		return FALSE;
	}
	if (!Socks5ProxyFilter::Effective((struct sockaddr_in *)name))
	{
		WSPClenupContext(s); // 清理与此套接字连接绑定的的上下文
		return Pfn_ConnectEx(s, name, namelen, lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
	}
	struct sockaddr_in server;
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(1080); // PORT
	server.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
	SocketBinderContext* binder = SocketMappingPool_Current.Get(s);
	if (binder != NULL)
	{
		binder->EnterLook();
		{
			binder->AddressFrmily = name->sa_family;
			binder->PeerNameLen = namelen;
			binder->PeerName = new BYTE[namelen];
			memcpy(binder->PeerName, name, namelen);
		}
		binder->LeaveLook();
	} // 如果采取的完成端口方式连接，无法立即返回始终返回FALSE 您可以通过WSAGetLastError()判断连接是否处于WSA_IO_PENDING等候状态
	return Pfn_ConnectEx(s, (struct sockaddr*)&server, sizeof(struct sockaddr_in), lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
}

INT PASCAL WSPSendMsg(
	SOCKET s,
	LPWSAMSG lpMsg,
	DWORD dwFlags,
	LPDWORD lpNumberOfBytesSent,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	INT err = 0;
	int error = Handshake(s, &err);
	if (error == 0)
	{
		if (Pfn_WSASendMsg == NULL)
			Pfn_WSASendMsg = (LPFN_WSASENDMSG)SocketExtension::GetExtensionFunction(s, WSAID_WSASENDMSG);
		if (Pfn_WSASendMsg == NULL)
			return SOCKET_ERROR;
		return Pfn_WSASendMsg(s, lpMsg, dwFlags, lpNumberOfBytesSent, lpOverlapped, lpCompletionRoutine);
	}
	return SOCKET_ERROR;
}

int WSPAPI WSPIoctl(
	_In_ SOCKET s,
	_In_ DWORD dwIoControlCode,
	_In_reads_bytes_opt_(cbInBuffer) LPVOID lpvInBuffer,
	_In_ DWORD cbInBuffer,
	_Out_writes_bytes_to_opt_(cbOutBuffer, *lpcbBytesReturned) LPVOID lpvOutBuffer,
	_In_ DWORD cbOutBuffer,
	_Out_ LPDWORD lpcbBytesReturned,
	_Inout_opt_ LPWSAOVERLAPPED lpOverlapped,
	_In_opt_ LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	_In_opt_ LPWSATHREADID lpThreadId,
	_Out_ LPINT lpErrno
)
{
	SupersocksRConfiguration* conf = SupersocksRInteractive_Current.Configuration();
	if (conf->EnableProxyClient)
	{
		if (dwIoControlCode == SIO_GET_EXTENSION_FUNCTION_POINTER && (lpvInBuffer != NULL && cbInBuffer >= sizeof(HANDLE)))
		{
			if (lpvOutBuffer != NULL && cbOutBuffer >= sizeof(HANDLE))
			{
				GUID connectex = WSAID_CONNECTEX;
				GUID sendmsg = WSAID_WSASENDMSG;
				if (memcmp(lpvInBuffer, &connectex, sizeof(GUID)) == 0)
				{
					*((LPFN_CONNECTEX*)lpvOutBuffer) = &ConnectEx;
					*lpcbBytesReturned = sizeof(HANDLE);
					return NO_ERROR;
				}
				else if (memcmp(lpvInBuffer, &sendmsg, sizeof(GUID)) == 0)
				{
					*((LPFN_WSASENDMSG*)lpvOutBuffer) = &WSPSendMsg;
					*lpcbBytesReturned = sizeof(HANDLE);
					return NO_ERROR;
				}
			}
		}
	}
	int error = LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer
		, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);;
	if (conf->EnableProxyClient)
	{
		if (error != SOCKET_ERROR && dwIoControlCode == FIONBIO && lpvInBuffer != NULL)
		{
			SocketBinderContext* binder = SocketMappingPool_Current.Get(s);
			if (binder != NULL)
			{
				binder->Nonblock = *(u_long*)lpvInBuffer;
			}
		}
	}
	return error;
}

int WSPAPI WSPSend(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesSent,
	DWORD dwFlags,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID lpThreadId,
	LPINT lpErrno
)
{
	int error = Handshake(s, lpErrno);
	if (error == NO_ERROR)
	{
		return LayeredServiceProvider_Current.NextProcTable.lpWSPSend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags,
			lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
	}
	return SOCKET_ERROR;
}

SOCKET WSPAPI WSPSocket(
	int af,
	int type,
	int protocol,
	LPWSAPROTOCOL_INFOW lpProtocolInfo,
	GROUP g,
	DWORD dwFlags,
	LPINT lpErrno
)
{
	TCHAR processname[MAX_PATH];
	GetModuleFileName(NULL, processname, MAX_PATH);
	Debugger::Write(L"%s WSPSocket ...", processname);

	SOCKET s = LayeredServiceProvider_Current.NextProcTable.lpWSPSocket(af, type, protocol, lpProtocolInfo, g, dwFlags, lpErrno);
	SupersocksRConfiguration* conf = SupersocksRInteractive_Current.Configuration();
	if (conf->EnableProxyClient && s != INVALID_SOCKET)
	{
		if(af == AF_INET && (type == SOCK_STREAM || protocol == IPPROTO_TCP))
		{
			SocketMappingPool_Current.Add(s);
		}
	}
	return s;
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

int WSPAPI WSPRecvFrom(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesRecvd,
	LPDWORD lpFlags,
	struct sockaddr* lpFrom,
	LPINT lpFromlen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID lpThreadId,
	LPINT lpErrno
)
{
	return LayeredServiceProvider_Current.NextProcTable.lpWSPRecvFrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
}

int WSPAPI WSPSendTo(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesSent,
	DWORD dwFlags,
	const struct sockaddr  * lpTo,
	int iTolen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID lpThreadId,
	LPINT lpErrno
)
{
	return LayeredServiceProvider_Current.NextProcTable.lpWSPSendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iTolen, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);;
}

int WSPAPI WSPCleanup(
	LPINT lpErrno
)
{
	return LayeredServiceProvider_Current.NextProcTable.lpWSPCleanup(lpErrno);
}

int WSPAPI WSPAsyncSelect(
	__in SOCKET s,
	__in HWND hWnd,
	__in unsigned int wMsg,
	__in long lEvent,
	__out LPINT lpErrno
)
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
				evt->hEvent = (__int64)lEvent;
				evt->hWnd = hWnd;
				evt->wMsg = wMsg;
				evt->lNetworkEvents = NULL;

				context->Events.push_back(evt);
				context->HWndList.insert(evt->hWnd);
			}
			context->LeaveLook();
		}
	}
	return LayeredServiceProvider_Current.NextProcTable.lpWSPAsyncSelect(s, hWnd, wMsg, lEvent, lpErrno);
}

void WSPAPI StartProviderCompleted(WSPPROC_TABLE* sender, WSPPROC_TABLE* e)
{
	e->lpWSPConnect = &WSPConnect;
	e->lpWSPShutdown = &WSPShutdown;
	e->lpWSPCloseSocket = &WSPCloseSocket;
	e->lpWSPIoctl = &WSPIoctl;
	e->lpWSPSend = &WSPSend;
	e->lpWSPSocket = &WSPSocket;
	e->lpWSPSendTo = &WSPSendTo;
	e->lpWSPRecvFrom = &WSPRecvFrom;
	e->lpWSPCleanup = &WSPCleanup;
	e->lpWSPEventSelect = &WSPEventSelect;
	e->lpWSPAsyncSelect = &WSPAsyncSelect;
	e->lpWSPGetPeerName = &WSPGetPeerName;
	e->lpWSPGetSockName = &WSPGetSockName;
};

//WSPStartup
int WSPAPI WSPStartup(
	WORD wversionrequested,
	LPWSPDATA lpwspdata,
	LPWSAPROTOCOL_INFOW lpProtoInfo,
	WSPUPCALLTABLE upcalltable,
	LPWSPPROC_TABLE lpproctable
)
{
	TCHAR processname[MAX_PATH];
	GetModuleFileName(NULL, processname, MAX_PATH);
	Debugger::Write(L"%s Loading WSPStartup ...", processname);

	LayeredServiceProvider_Current.StartProviderCompleted = &StartProviderCompleted;
	return LayeredServiceProvider_Current.Start(MAKEWORD(2, 2), lpwspdata, lpProtoInfo, upcalltable, lpproctable);
};