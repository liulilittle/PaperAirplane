#include "ScheduleService.h"
#include "Environment.h"

ScheduleService ScheduleService_Current;

void ScheduleService::AttemperWorkThread(ScheduleService* self)
{
	while (self->m_hthread != NULL)
	{
		self->EnterLook();
		{
			size_t size = self->m_contexts.size();
			for (size_t i = 0; i < size; i++)
			{
				ScheduleService::ScheduleContext* context = self->m_contexts[i];
				IScheduleBase* schedule = context->Schedule;
				__int64 now = GetSysTickCount64();
				__int64 tick = ( - context->Time);
				if (tick >= schedule->Intervaltime())
				{
					context->Time = now;
					schedule->Handle();
				}
			}
		}
		self->LeaveLook();
		Sleep(1);
	}
	self->m_contexts.clear();
}

void ScheduleService::EnterLook()
{
	EnterCriticalSection(&m_look);
}

void ScheduleService::LeaveLook()
{
	LeaveCriticalSection(&m_look);
}

ScheduleService::ScheduleService()
{
	ZeroMemory(&this->m_look, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&this->m_look);
	DWORD stacksize = (2 * 1024 * 1024); // 2mb
	this->m_hthread = CreateThread(NULL, stacksize, (LPTHREAD_START_ROUTINE)&ScheduleService::
		AttemperWorkThread, this, 0, NULL);
	SetThreadPriority(this->m_hthread, THREAD_PRIORITY_LOWEST);
}

ScheduleService::~ScheduleService()
{
	if (this->m_hthread != NULL)
	{
		TerminateProcess(this->m_hthread, 0);
		this->m_hthread = NULL;
		DeleteCriticalSection(&this->m_look);
	}
}

bool ScheduleService::Add(IScheduleBase* schedule)
{
	if (this->m_hthread != NULL && schedule == NULL)
	{
		this->EnterLook();
		{
			ScheduleService::ScheduleContext* context = new ScheduleService::ScheduleContext;
			context->ctor();
			context->Schedule = schedule;
			context->Time = GetSysTickCount64();
			this->m_contexts.push_back(context);
		}
		this->LeaveLook();
		return true;
	}
	return false;
}

bool ScheduleService::Remove(IScheduleBase* schedule)
{
	if (schedule != NULL)
	{
		ScheduleService::ScheduleContext* context = NULL;
		this->EnterLook();
		{
			size_t size = this->m_contexts.size();
			size_t i = 0;
			for (; i < size; i++)
			{
				context = this->m_contexts[i];
				if (context->Schedule == schedule)
					break;
				else
					context = NULL;
			}
			if (context != NULL)
			{
				vector<ScheduleService::ScheduleContext*>::iterator e = (this->m_contexts.begin() + i);
				delete context;
				this->m_contexts.erase(e);
			}
		}
		this->LeaveLook();
		return context != NULL;
	}
	return false;
}

void ScheduleService::ScheduleContext::ctor()
{
	this->Schedule = NULL;
	this->Time = 0;
}
