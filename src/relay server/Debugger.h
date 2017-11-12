#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "Environment.h"

class Debugger
{
public:
	static int Write(LPCWSTR fmt, ...);

	static int Write(LPCSTR fmt, ...);
};

#endif