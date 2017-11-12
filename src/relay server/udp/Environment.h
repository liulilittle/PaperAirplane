#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "iphlpapi.lib")

#include <ws2spi.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <shlwapi.h>
#include <iphlpapi.h>
#include <malloc.h>
#include <iostream>
#include <hash_map>
#include <hash_set>
#include <vector>
#include <string>

using namespace std;

__int64 GetSysTickCount64();
int GetProcessorCoreCount();

#endif
