#ifndef SUPERSOCKSRINTERACTIVE_H
#define SUPERSOCKSRINTERACTIVE_H

#include "SocketExtension.h"

#ifndef PAPERAIRPLANE_CONFIG_WRITE_CHANNEL
#define PAPERAIRPLANE_CONFIG_WRITE_CHANNEL "PaperAirplane.Configuration_WriteMmfMap"
#endif
#ifndef PAPERAIRPLANE_CONFIG_READ_CHANNEL
#define PAPERAIRPLANE_CONFIG_READ_CHANNEL "PaperAirplane.Configuration_ReadMmfMap"
#endif

typedef struct SupersocksRConfiguration
{
	BYTE EnableProxyClient; // 启用代理客户端
	BYTE ResolveDNSRemote; // 远程地址解析
	USHORT ProxyHostPort; // 代理端口
	ULONG ProxyHostAddress; // 代理地址
	hash_set<USHORT> FilterPortNumber; // 过滤端口号
	hash_set<ULONG> FilterHostAddress; // 过滤端口号
	hash_set<string> FilterHostName; // 过滤主机域名
} SupersocksRConfiguration;

typedef enum SupersocksRInteractiveCommands : UINT8
{
	SupersocksRInteractiveCommands_QueryConfiguration = 1,
	SupersocksRInteractiveCommands_ConnectionHeartbeat = 2,
} SupersocksRInteractiveCommands;

class SupersocksRInteractive 
{
private:
	HANDLE m_hthread;
	SupersocksRConfiguration m_configuration;
	CRITICAL_SECTION m_cs;
	HANDLE m_hReadMap;
	HANDLE m_hReadEvent;
	void* m_pReadMView;
	INT m_msgID;

public:
	SupersocksRInteractive();

	~SupersocksRInteractive();

	bool Enter();

	void Leave();

	SupersocksRConfiguration* Configuration();

private:
	void ResetConfiguration();

	static DWORD WINAPI MMapWorkThread(SupersocksRInteractive* self);

	void HandleQueryConfiguration(char* buf);

	char* mmap(int timeout = INFINITE, int* error = NULL);

	void munmap();
};
#endif