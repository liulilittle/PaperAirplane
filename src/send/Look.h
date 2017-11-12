#ifndef LOOK_H
#define LOOK_H

#include "Environment.h"

class Look
{
private:
	CRITICAL_SECTION m_cs;

public:
	Look();
	~Look();

	bool EnterLook();

	void LeaveLook();
};
#endif

