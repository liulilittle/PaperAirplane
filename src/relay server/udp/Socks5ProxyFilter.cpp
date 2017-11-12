#include "Socks5ProxyFilter.h"
#include "PaperAirplaneInteractive.h"

extern PaperAirplaneInteractive PaperAirplaneInteractive_Current;

bool Socks5ProxyFilter::Effective(const struct sockaddr* addr)
{
	switch (addr->sa_family)
	{
	case AF_INET:
		struct sockaddr_in* sin = (struct sockaddr_in*)addr;
		{
			PaperAirplaneConfiguration* conf = PaperAirplaneInteractive_Current.Configuration();
			if (!Effective(sin->sin_addr.s_addr))
				return FALSE;
		}
		return Effective(sin->sin_port);
		break;
	}
	return FALSE;
}

bool Socks5ProxyFilter::Effective(ULONG addr)
{
	PaperAirplaneConfiguration* conf = PaperAirplaneInteractive_Current.Configuration();
	if (!conf->EnableProxyClient)
		return FALSE;
	UINT8* p = reinterpret_cast<UINT8*>(&addr);
	/* PRIVATE ADDRESS SEGMENT
		localhost;
		127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;172.23.*;172.24.*;
		172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;172.32.*;192.168.*;<local>
	*/
	if (*p == 0 || (*p == 127 && p[1] != 8)) // I / L
		return FALSE;
	if (*p > 223 || *p == 10) // A
		return FALSE;
	if (*p == 172 && (p[1] >= 16 && p[1] <= 32)) // B
		return FALSE;
	if (*p == 192 && p[1] == 168) // C
		return FALSE;
	bool effective = TRUE;
	if(PaperAirplaneInteractive_Current.Enter())
	{
		hash_set<ULONG>::iterator i = conf->FilterHostAddress.find(ntohl(addr));
		effective = (i == conf->FilterHostAddress.end());
	}
	PaperAirplaneInteractive_Current.Leave();
	return effective;
}

bool Socks5ProxyFilter::Effective(USHORT port)
{
	PaperAirplaneConfiguration* conf = PaperAirplaneInteractive_Current.Configuration();
	if (!conf->EnableProxyClient)
		return FALSE;
	bool effective = TRUE;
	if(PaperAirplaneInteractive_Current.Enter())
	{
		hash_set<USHORT>::iterator i = conf->FilterPortNumber.find(port);
		effective = (i == conf->FilterPortNumber.end());
	}
	PaperAirplaneInteractive_Current.Leave();
	return effective;
}

bool Socks5ProxyFilter::Effective(string hostname)
{
	PaperAirplaneConfiguration* conf = PaperAirplaneInteractive_Current.Configuration();
	if (!conf->EnableProxyClient)
		return FALSE;
	bool effective = TRUE;
	if(PaperAirplaneInteractive_Current.Enter())
	{
		hash_set<string>::iterator i = conf->FilterHostName.find(hostname);
		effective = (i == conf->FilterHostName.end());
	}
	PaperAirplaneInteractive_Current.Leave();
	return effective;
}