#pragma once

#include <Winsock2.h> 
#include <Windows.h> 
#include <Ws2spi.h> 
#include <iostream>
#include "Iphlpapi.h"
#include <Sporder.h>      // 定义了WSCWriteProviderOrder函数 

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Rpcrt4.lib")  // 实现了UuidCreate函数 

BOOL InstallLayeredServiceProvider(WCHAR *pwszPathName);
BOOL UninstallLayeredServiceProvider();