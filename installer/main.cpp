#include "lsp.h"
#include "nsp.h"

#include <string>
#include <map>

using namespace std;

BOOL IsWow64System()
{
	SYSTEM_INFO stInfo = { 0 };
	GetNativeSystemInfo(&stInfo);
	if (stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64
		|| stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
	{
		return TRUE;
	}
	return FALSE;
}

void install()
{
	WCHAR path[MAX_PATH];
	if (!IsWow64System())
		GetSystemDirectoryW(path, MAX_PATH);
	else
	{
		if (sizeof(HANDLE) == 4)
			GetSystemWow64DirectoryW(path, MAX_PATH);
		else
			GetSystemDirectoryW(path, MAX_PATH);
	}
	wcscat(path, L"\\PaperAirplane.dll");
	if (!InstallLayeredServiceProvider(path))
		printf("Unable to install LSP\n");
	else
		printf("The LSP has been install successfully\n");

	if (!InstallNamespaceServiceProvider(path))
		printf("Unable to install NSP\n");
	else
		printf("The NSP has been install successfully\n");

	system("pause");
}

void uninstall()
{

	if (!UninstallLayeredServiceProvider())
		printf("Unable to uninstall LSP\n");
	else
		printf("The LSP has been uninstall successfully\n");

	if (!UninstallNamespaceServiceProvider())
		printf("Unable to uninstall NSP\n");
	else
		printf("The NSP has been uninstall successfully\n");

	system("pause");
}

void usage(char *progname)
{
	printf("usage: %s install | uninstall\n", progname);
	system("pause");
	ExitProcess(-1);
}

int main(int argc, char** argv)
{
	WSADATA        wsd;
	char          *ptr;
	if (argc != 2)
	{
		usage(argv[0]);
		return -1;
	}
	if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
	{
		printf("WSAStartup() failed: %d\n", GetLastError());
		return -1;
	}
	ptr = argv[1];
	while (*ptr)
		*ptr++ = tolower(*ptr);
	if (!strncmp(argv[1], "install", 6))
	{
		install();
	}
	else if (!strncmp(argv[1], "uninstall", 6))
	{
		uninstall();
	}
	else
	{
		usage(argv[0]);
	}
	WSACleanup();
	return 0;
}