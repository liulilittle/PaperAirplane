#ifndef PAPERAIRPLANEINTERACTIVE_H
#define PAPERAIRPLANEINTERACTIVE_H

#include "SocketExtension.h"
#include "MMap.h"

#ifndef PAPERAIRPLANE_CONFIGURATION_MAP
#define PAPERAIRPLANE_CONFIGURATION_MAP "PAPERAIRPLANE_CONFIGURATION_MAP"
#endif
#ifndef PAPERAIRPLANE_CONFIGURATION_EVENT
#define PAPERAIRPLANE_CONFIGURATION_EVENT "PAPERAIRPLANE_CONFIGURATION_EVENT"
#endif

#define PAPERAIRPLANE_ADDRESSFRAMILY_INET (1 << 0)
#define PAPERAIRPLANE_ADDRESSFRAMILY_INET6 (1 << 1)
#define PAPERAIRPLANE_ADDRESSFRAMILY_HOSTNAME (1 << 2)
#define PAPERAIRPLANE_PROTOCOLTYPE_TCP (1 << 3)
#define PAPERAIRPLANE_PROTOCOLTYPE_UDP (1 << 4)

#define PAPERAIRPLANE_PROXECONNECT_MESSAGE 0x1311
#define PAPERAIRPLANE_PROXEBINDCONN_MESSAGE 0x1312
#define PAPERAIRPLANE_PROXYCLOSECONN_MESSAGE 0x1313

typedef struct PaperAirplaneConfiguration
{
	BYTE EnableProxyClient; // 启用代理客户端
	BYTE ResolveDNSRemote; // 远程地址解析
	INT32 ProxyNotifyHWnd; // 代理窗口
	hash_set<USHORT> FilterPortNumber; // 过滤端口号
	hash_set<ULONG> FilterHostAddress; // 过滤端口号
	hash_set<string> FilterHostName; // 过滤主机域名
} PaperAirplaneConfiguration;

typedef void(CALLBACK* PaperAirplaneDisconnectedEventHandler)();
typedef void(CALLBACK* PaperAirplaneConnectedEventHandler)();

class PaperAirplaneInteractive 
{
private:
	HANDLE m_hthread;
	PaperAirplaneConfiguration m_configuration;
	CRITICAL_SECTION m_cs;
	HANDLE m_hMap;
	HANDLE m_hEvent;
	void* m_pLow;
	INT m_msgid; 
	DWORD m_processid;
	BOOL m_available;

public:
	PaperAirplaneDisconnectedEventHandler Disconnected;
	PaperAirplaneConnectedEventHandler Connected;

	PaperAirplaneInteractive();

	~PaperAirplaneInteractive();

	bool Enter();

	void Leave();

	bool Available();

	PaperAirplaneConfiguration* Configuration();

	BOOL ProxyConnect(SOCKET s, BYTE* name, int namelen, USHORT nameport, int atype, int* outport);
	BOOL ProxyBind(SOCKET s, int* outport);
	BOOL ProxyClose(SOCKET s);

private:
	void ResetConfiguration();

	static DWORD WINAPI MMapWorkThread(PaperAirplaneInteractive* self);

	void HandleQueryConfiguration(char* buf);

	char* mmap(int timeout = INFINITE, int* error = NULL);

	void munmap();
};
#endif