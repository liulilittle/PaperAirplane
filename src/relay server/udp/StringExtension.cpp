#include "StringExtension.h"

LPCSTR StringExtension::W2A(LPWSTR str)
{
	if (str == NULL)
	{
		return NULL;
	}
	int size = WideCharToMultiByte(936, 0, str, -1, NULL, 0, NULL, NULL);
	if (size <= 0)
	{
		return NULL;
	}
	char* tmp = new char[size + 1];
	WideCharToMultiByte(936, 0, str, -1, tmp, size, NULL, NULL);
	return tmp;
}

LPWSTR StringExtension::A2W(LPSTR str)
{
	if (str == NULL)
	{
		return NULL;
	}
	int size = MultiByteToWideChar(936, 0, str, -1, NULL, 0);
	if (size <= 0)
	{
		return NULL;
	}
	wchar_t *tmp = new wchar_t[size + 1];
	MultiByteToWideChar(936, 0, str, -1, tmp, size * sizeof(wchar_t));
	return tmp;
}
