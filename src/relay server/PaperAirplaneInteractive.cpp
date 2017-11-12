#pragma warning(disable: 4312)
#pragma warning(disable: 4800)

#include "PaperAirplaneInteractive.h"
#include "Debugger.h"
#include "SocketExtension.h"

PaperAirplaneInteractive PaperAirplaneInteractive_Current;

PaperAirplaneInteractive::PaperAirplaneInteractive()
{
	memset(&this->m_cs, 0, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(&this->m_cs);

	this->Connected = NULL;
	this->Disconnected = NULL;

	this->m_hEvent = NULL;
	this->m_hMap = NULL;
	this->m_pLow = NULL;
	this->m_msgid = 0;
	this->m_processid = GetCurrentProcessId();

	this->m_available = FALSE;
	this->m_hthread = NULL;
	this->ResetConfiguration();

	char* packet = this->mmap(500);
	if (packet != NULL) {
		this->m_available = TRUE;
		this->HandleQueryConfiguration(packet);
	}

	this->m_hthread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&PaperAirplaneInteractive::
		MMapWorkThread, this, 0, NULL);
	SetThreadPriority(this->m_hthread, THREAD_PRIORITY_LOWEST);
}

PaperAirplaneInteractive::~PaperAirplaneInteractive()
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

bool PaperAirplaneInteractive::Enter()
{
	EnterCriticalSection(&this->m_cs);
	return TRUE;
}

void PaperAirplaneInteractive::Leave()
{
	LeaveCriticalSection(&this->m_cs);
}

bool PaperAirplaneInteractive::Available()
{
	return m_available;
}

void PaperAirplaneInteractive::ResetConfiguration()
{
	if (this->Enter())
	{
		this->m_configuration.EnableProxyClient = FALSE;
		this->m_configuration.ResolveDNSRemote = FALSE;

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

DWORD PaperAirplaneInteractive::MMapWorkThread(PaperAirplaneInteractive* self)
{
	BOOL mmfopen = FALSE;
	do
	{
		__try {
			int error;
			char* conf = self->mmap(500, &error);
			if (error == WAIT_FAILED) {
				if (mmfopen) {
					self->ResetConfiguration();
					mmfopen = FALSE;
					self->m_msgid = 0;
					if (self->Disconnected != NULL) {
						self->Disconnected();
					}
				}
				Sleep(200);
			}
			else {
				if (conf != NULL) {
					self->HandleQueryConfiguration(conf);
				}
				if (mmfopen != TRUE) {
					if (self->Connected != NULL) {
						self->Connected();
					}
				}
				mmfopen = TRUE;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			self->munmap();
			self->ResetConfiguration();
			mmfopen = FALSE;
		}
	} while (TRUE);
	return 0;
}

void PaperAirplaneInteractive::HandleQueryConfiguration(char* buf)
{
	INT msgid;
	if (buf != NULL && (msgid = *(INT*)buf) != this->m_msgid && this->Enter())
	{
		this->m_msgid = msgid;
		buf += 4;
		this->ResetConfiguration();
		PaperAirplaneConfiguration* conf = &this->m_configuration;
		conf->EnableProxyClient = *buf++;
		if (conf->EnableProxyClient)
		{
			conf->ResolveDNSRemote = *buf++;
			conf->ProxyNotifyHWnd = *(INT32*)buf;
			buf += 4;
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

char* PaperAirplaneInteractive::mmap(int timeout, int* error)
{
	PaperAirplaneInteractive* self = this;
	BOOL cleanup = TRUE;
	do
	{
		if (self->m_hMap == NULL) {
			self->m_hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, PAPERAIRPLANE_CONFIGURATION_MAP);
		}
		if (self->m_hEvent == NULL) {
			self->m_hEvent = OpenEventA(EVENT_ALL_ACCESS, FALSE, PAPERAIRPLANE_CONFIGURATION_EVENT);
		}
		if (self->m_hMap != NULL && self->m_pLow == NULL) {
			self->m_pLow = MapViewOfFile(self->m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		}
		if (self->m_hEvent == NULL || self->m_pLow == NULL) {
			if (error != NULL)
				*error = WAIT_FAILED;
			break;
		}
		int err = WaitForSingleObject(self->m_hEvent, timeout);
		if (err != WAIT_OBJECT_0) {
			PaperAirplaneConfiguration* conf = self->Configuration();
			if (error != NULL)
				*error = IsWindow((HWND)conf->ProxyNotifyHWnd) ? WAIT_TIMEOUT : WAIT_FAILED;
			break;
		}
		if (error != NULL) {
			*error = WAIT_OBJECT_0;
		}
		cleanup = FALSE;
		ResetEvent(self->m_hEvent);
	} while (0, 0);
	if (cleanup) {
		self->munmap();
	}
	return (char*)self->m_pLow;
}

void PaperAirplaneInteractive::munmap()
{
	PaperAirplaneInteractive* self = this;
	if (self->Enter())
	{
		if (self->m_pLow != NULL)
			UnmapViewOfFile(self->m_pLow);
		if (self->m_hMap != NULL)
			CloseHandle(self->m_hMap);
		if (self->m_hEvent != NULL)
			CloseHandle(self->m_hEvent);
		self->m_hMap = NULL;
		self->m_pLow = NULL;
		self->m_hEvent = NULL;
	}
	self->Leave();
}

PaperAirplaneConfiguration* PaperAirplaneInteractive::Configuration()
{
	return &this->m_configuration;
}

BOOL PaperAirplaneInteractive::ProxyConnect(SOCKET s, BYTE* name, int namelen, USHORT nameport, int atype, int* outport)
{
	if (s == INVALID_SOCKET || outport == NULL)
	{
		return FALSE;
	}
	if (name == NULL || namelen <= 0 || nameport == 0)
	{
		return FALSE;
	}
	*outport = 0;
	char szname[MAXBYTE];
	sprintf(szname, "PAPERAIRPLANE%dPROXYCONNECT%d", this->m_processid, (UINT)s);
	MMap mmap;
	mmap.Bind(szname, 272);
	{
		char* p = (char*)mmap.GetFirstPtr();
		*p++ = atype;
		*(USHORT*)p = nameport;
		p += 2;
		if (atype & PAPERAIRPLANE_ADDRESSFRAMILY_HOSTNAME)
		{
			*p++ = namelen;
		}
		memcpy(p, name, namelen);
		*outport = (INT)SendMessage((HWND)m_configuration.ProxyNotifyHWnd, PAPERAIRPLANE_PROXECONNECT_MESSAGE, this->m_processid, s);
	}
	mmap.Close();
	return *outport != 0;
}

BOOL PaperAirplaneInteractive::ProxyBind(SOCKET s, int* outport)
{
	if (s == INVALID_SOCKET || outport == NULL)
		return FALSE;
	*outport = (INT)SendMessage((HWND)m_configuration.ProxyNotifyHWnd, PAPERAIRPLANE_PROXEBINDCONN_MESSAGE, this->m_processid, s);
	return *outport != 0;
}

BOOL PaperAirplaneInteractive::ProxyClose(SOCKET s)
{
	if (s == INVALID_SOCKET)
		return FALSE;
	return (BOOL)SendMessage((HWND)m_configuration.ProxyNotifyHWnd, PAPERAIRPLANE_PROXYCLOSECONN_MESSAGE, this->m_processid, s);
}
