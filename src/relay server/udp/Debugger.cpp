#include "Debugger.h"

int Debugger::Write(LPCWSTR fmt, ...)
{
	WCHAR message[1024];
	int len = wvsprintfW(message, fmt, va_list(&fmt + 1));
	OutputDebugStringW(message);
	return len;
}
int Debugger::Write(LPCSTR fmt, ...)
{
	CHAR message[1024];
	int len = wvsprintfA(message, fmt, va_list(&fmt + 1));
	OutputDebugStringA(message);
	return len;
}
