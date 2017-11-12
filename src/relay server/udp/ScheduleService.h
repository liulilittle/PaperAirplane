#ifndef SCHEDULESERVICE_H
#define SCHEDULESERVICE_H

#include "Environment.h"

class IScheduleBase
{
public:
	virtual int Intervaltime() = 0;
	virtual void Handle() = 0;
};

class ScheduleService
{
private:
	typedef struct ScheduleContext
	{
		IScheduleBase* Schedule;
		__int64 Time;

		void ctor();
	} ScheduleContext;

	vector<ScheduleService::ScheduleContext*> m_contexts;
	CRITICAL_SECTION m_look;
	HANDLE m_hthread;

	static void AttemperWorkThread(ScheduleService* state);

	void EnterLook();

	void LeaveLook();

public:
	ScheduleService();
	~ScheduleService();

	bool Add(IScheduleBase* schedule);

	bool Remove(IScheduleBase* schedule);
};
#endif
