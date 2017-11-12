#ifndef STRINGEXTENSION_H
#define STRINGEXTENSION_H

#include "Environment.h"

class StringExtension
{
public:
	static LPCSTR W2A(LPWSTR str);

	static LPWSTR A2W(LPSTR str);
};
#endif

