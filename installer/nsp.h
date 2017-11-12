#pragma once
#include <winsock2.h>
#include <ws2spi.h>

#pragma comment(lib, "ws2_32.lib")

BOOL InstallNamespaceServiceProvider(LPWSTR provider);
BOOL UninstallNamespaceServiceProvider();