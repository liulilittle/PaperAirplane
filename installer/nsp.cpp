#include "nsp.h"

GUID MY_NAMESPACE_GUID = { 0x55a2bd9e, 0xbb30, 0x11d2, { 0x91,0x66,0x00,0xa0,0xc9,0xa7,0x86,0xe8 } };

BOOL InstallNamespaceServiceProvider(LPWSTR provider)
{
	return WSCInstallNameSpace(L"PaperAirplane Namespace Service Provider", provider, NS_DNS, 1, &MY_NAMESPACE_GUID) != SOCKET_ERROR;
}

BOOL UninstallNamespaceServiceProvider()
{
	return WSCUnInstallNameSpace(&MY_NAMESPACE_GUID) != SOCKET_ERROR;
}
