#include "SupersocksRInteractive.h"
#include "Debugger.h"
#include "SocketExtension.h"

SupersocksRInteractive SupersocksRInteractive_Current;

SupersocksRInteractive::SupersocksRInteractive()
{
	memset(&this->m_cs, 0, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&this->m_cs);

	this->m_hReadEvent = NULL;
	this->m_hReadMap = NULL;
	this->m_pReadMView = NULL;
	this->m_msgID = 0;

	this->m_hthread = NULL;
	this->ResetConfiguration();
	this->HandleQueryConfiguration(this->mmap(200));

	this->m_hthread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&SupersocksRInteractive::
		MMapWorkThread, this, 0, NULL);
	SetThreadPriority(this->m_hthread, THREAD_PRIORITY_LOWEST);
}

SupersocksRInteractive::~SupersocksRInteractive()
{
	this->Enter();
	{
		if (this->m_hthread != NULL)
		{
			TerminateThread(this->m_hthread, 0);
			this->m_hthread = NULL;
		}
		this->munmap();
	}
	this->Leave();
	DeleteCriticalSection(&m_cs);
}

bool SupersocksRInteractive::Enter()
{
	EnterCriticalSection(&this->m_cs);
	return TRUE;
}

void SupersocksRInteractive::Leave()
{
	LeaveCriticalSection(&this->m_cs);
}

void SupersocksRInteractive::ResetConfiguration()
{
	if (this->Enter())
	{
		this->m_configuration.EnableProxyClient = FALSE;
		this->m_configuration.ResolveDNSRemote = FALSE;
		this->m_configuration.ProxyHostPort = 1080;
		this->m_configuration.ProxyHostAddress = INADDR_LOOPBACK;

		this->m_configuration.FilterHostAddress.clear();
		this->m_configuration.FilterHostName.clear();
		this->m_configuration.FilterPortNumber.clear();

		vector<ULONG>* localEPs = SocketExtension::LocalIPs();
		if (localEPs != NULL)
		{
			for each (ULONG localEP in *localEPs)
			{
				this->m_configuration.FilterHostAddress.insert(localEP);
			}
			delete localEPs;
		}
	}
	this->Leave();
}

DWORD SupersocksRInteractive::MMapWorkThread(SupersocksRInteractive* self)
{
	BOOL mmfopen = FALSE;
	do
	{
		int error;
		char* conf = self->mmap(200, &error);
		if (error == WAIT_FAILED)
		{
			if (mmfopen)
			{
				mmfopen = FALSE;
				self->m_msgID = 0;
				self->ResetConfiguration();
			}
			Sleep(300);
		}
		else
		{
			if (conf != NULL)
				self->HandleQueryConfiguration(conf);
			mmfopen = TRUE;
		}
	} while (TRUE);
	return 0;
}

void SupersocksRInteractive::HandleQueryConfiguration(char* buf)
{
	INT msgid;
	if (buf != NULL && (msgid = *(INT*)buf) != this->m_msgID && this->Enter())
	{
		this->m_msgID = msgid;
		buf += 4;
		this->ResetConfiguration();
		SupersocksRConfiguration* conf = &this->m_configuration;
		memcpy(conf, buf, 8);
		if (conf->EnableProxyClient)
		{
			buf += 8;
			INT len = *(USHORT*)buf;
			buf += 2;
			for (INT i = 0; i < len; i++)
			{
				USHORT port = *(USHORT*)buf;
				conf->FilterPortNumber.insert(port);
				buf += 2;
			}
			len = *(USHORT*)buf;
			buf += 2;
			for (INT i = 0; i < len; i++)
			{
				ULONG address = *(ULONG*)buf;
				conf->FilterHostAddress.insert(address);
				buf += 4;
			}
			len = *(USHORT*)buf;
			buf += 2;
			for (INT i = 0; i < len; i++)
			{
				INT cb = *(USHORT*)buf;
				buf += 2;
				conf->FilterHostName.insert(strlwr(buf));
				buf += cb;
			}
			if (conf->EnableProxyClient)
			{
				char module[4096];
				if (GetModuleFileNameA(NULL, module, 4096))
				{
					len = *(USHORT*)buf;
					buf += 2;
					for (INT i = 0; i < len; i++)
					{
						INT cb = *(USHORT*)buf;
						buf += 2;
						if (stricmp(module, buf) == 0)
						{
							conf->EnableProxyClient = FALSE;
						}
						buf += cb;
					}
				}
			}
		}
		this->Leave();
	}
}

char* SupersocksRInteractive::mmap(int timeout, int* error)
{
	SupersocksRInteractive* self = this;
	BOOL cleanup = TRUE;
	do
	{
		if (self->m_hReadMap == NULL && (self->m_hReadMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, PAPERAIRPLANE_CONFIG_WRITE_CHANNEL)) == NULL)
		{
			self->m_hReadMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, (LPSTR)TEXT(PAPERAIRPLANE_CONFIG_WRITE_CHANNEL));
		}
		if (self->m_hReadEvent == NULL)
		{
			self->m_hReadEvent = OpenEventA(EVENT_ALL_ACCESS, FALSE, PAPERAIRPLANE_CONFIG_WRITE_CHANNEL);
		}
		if (self->m_hReadMap != NULL && self->m_pReadMView == NULL)
		{
			self->m_pReadMView = MapViewOfFile(self->m_hReadMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		}
		if (self->m_hReadEvent == NULL || self->m_pReadMView == NULL)
		{
			if (error != NULL)
				*error = WAIT_FAILED;
			break;
		}
		if (WaitForSingleObject(self->m_hReadEvent, timeout) != WAIT_OBJECT_0)
		{
			if (error != NULL)
				*error = WAIT_TIMEOUT;
			break;
		}
		if (error != NULL)
			*error = WAIT_OBJECT_0;
		cleanup = FALSE;
		ResetEvent(self->m_hReadEvent);
	} while (0);
	if (cleanup)
	{
		self->munmap();
	}
	return (char*)self->m_pReadMView;
}

void SupersocksRInteractive::munmap()
{
	SupersocksRInteractive* self = this;
	if (self->Enter())
	{
		if (self->m_pReadMView != NULL)
			UnmapViewOfFile(self->m_pReadMView);
		if (self->m_hReadMap != NULL)
			CloseHandle(self->m_hReadMap);
		if (self->m_hReadEvent != NULL)
			CloseHandle(self->m_hReadEvent);
		self->m_hReadMap = NULL;
		self->m_pReadMView = NULL;
		self->m_hReadEvent = NULL;
	}
	self->Leave();
}

SupersocksRConfiguration* SupersocksRInteractive::Configuration()
{
	return &this->m_configuration;
}
