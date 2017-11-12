#include "lsp.h"

GUID PAPERAIRPLANE_MAIN_GUID = (sizeof(HANDLE) < 8 ? GUID({ 0x70b2b755, 0xa09d, 0x4b5d,{ 0xba, 0xda, 0xdb, 0x70, 0xbb, 0x1a, 0xbb, 0x21 } }) :
	GUID({ 0x51361ede, 0xe7c4, 0x4598, { 0xa1, 0x77, 0xf4, 0xc5, 0xe9, 0x1, 0x7c, 0x25 } }));
GUID PAPERAIRPLANE_NET32_GUID = { 0x15d3a7df, 0x9be4, 0x49fb,{ 0xa6, 0x66, 0x7e, 0x3, 0x94, 0xe1, 0x2d, 0x6f } };
GUID PAPERAIRPLANE_NET64_GUID = { 0x42e92cbe, 0xf23f, 0x475f,{ 0x9e, 0x23, 0x34, 0x88, 0xd8, 0xaf, 0x2b, 0x36 } };

LPWSAPROTOCOL_INFOW LoadLayeredServiceProvider(LPINT lpnTotalProtocols)
{
	DWORD dwSize = 0;
	int nError;
	LPWSAPROTOCOL_INFOW pProtoInfo = NULL;
	// 取得需要的长度 
	if (::WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError) == SOCKET_ERROR) {
		if (nError != WSAENOBUFS)
			return NULL;
	}
	pProtoInfo = (LPWSAPROTOCOL_INFOW)::GlobalAlloc(GPTR, dwSize);
	*lpnTotalProtocols = ::WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError);
	return pProtoInfo;
}

void FreeLayeredServiceProvider(LPWSAPROTOCOL_INFOW pProtoInfo)
{
	::GlobalFree(pProtoInfo);
}

BOOL InstallLayeredServiceProvider(WCHAR *pwszPathName)
{
	WCHAR wszLSPName[] = L"PaperAirplane";
	LPWSAPROTOCOL_INFOW pProtoInfo;
	int nProtocols;
	int nArrayCount = 0;
	DWORD dwLayeredCatalogId;       // 我们分层协议的目录ID号 
	int nError;
	WSAPROTOCOL_INFOW OriginalProtocolInfo[MAXBYTE];
	DWORD            dwOrigCatalogId[MAXBYTE];
	// 找到我们的下层协议，将信息放入数组中 
	// 枚举所有服务程序提供者 
	pProtoInfo = LoadLayeredServiceProvider(&nProtocols);
	for (int i = 0; i < nProtocols; i++) {
		int af = pProtoInfo[i].iAddressFamily;
		if (af == AF_INET || af == AF_INET6) {
			WSAPROTOCOL_INFOW* proto = &OriginalProtocolInfo[nArrayCount];
			memcpy(proto, &pProtoInfo[i], sizeof(WSAPROTOCOL_INFOW));
			proto->dwServiceFlags1 =
				proto->dwServiceFlags1 & (~XP1_IFS_HANDLES);
			dwOrigCatalogId[nArrayCount++] = pProtoInfo[i].dwCatalogEntryId;
		}
	}
	// 安装我们的分层协议，获取一个dwLayeredCatalogId 
	// 随便找一个下层协议的结构复制过来即可 
	WSAPROTOCOL_INFOW LayeredProtocolInfo;
	memcpy(&LayeredProtocolInfo, &OriginalProtocolInfo[0], sizeof(WSAPROTOCOL_INFOW));
	// 修改协议名称，类型，设置PFL_HIDDEN标志 
	wcscpy(LayeredProtocolInfo.szProtocol, wszLSPName);
	LayeredProtocolInfo.ProtocolChain.ChainLen = LAYERED_PROTOCOL; // 0; 
	LayeredProtocolInfo.dwProviderFlags |= PFL_HIDDEN;
	// 安装 
	nError = 0;
	if (::WSCInstallProvider(&PAPERAIRPLANE_MAIN_GUID, pwszPathName, &LayeredProtocolInfo, 1, &nError) == SOCKET_ERROR) {
		return FALSE;
	}
	// 重新枚举协议，获取分层协议的目录ID号 
	FreeLayeredServiceProvider(pProtoInfo);
	pProtoInfo = LoadLayeredServiceProvider(&nProtocols);
	for (int i = 0; i < nProtocols; i++) {
		if (memcmp(&pProtoInfo[i].ProviderId, &PAPERAIRPLANE_MAIN_GUID, sizeof(GUID)) == 0) {
			dwLayeredCatalogId = pProtoInfo[i].dwCatalogEntryId;
			break;
		}
	}
	// 安装协议链 
	// 修改协议名称，类型 
	WCHAR wszChainName[WSAPROTOCOL_LEN + 1];
	for (int i = 0; i < nArrayCount; i++) {
		WSAPROTOCOL_INFOW* proto = &OriginalProtocolInfo[i];
		swprintf(wszChainName, L"%ws %ws", wszLSPName, proto->szProtocol);
		wcscpy(proto->szProtocol, wszChainName);
		if (proto->ProtocolChain.ChainLen == 1) {
			proto->ProtocolChain.ChainEntries[1] = dwOrigCatalogId[i];
		}
		else {
			for (int j = proto->ProtocolChain.ChainLen; j > 0; j--) {
				proto->ProtocolChain.ChainEntries[j]
					= proto->ProtocolChain.ChainEntries[j - 1];
			}
		}
		proto->ProtocolChain.ChainLen++;
		proto->ProtocolChain.ChainEntries[0] = dwLayeredCatalogId;
	}
	// 获取一个Guid，安装之 
	GUID ProviderChainGuid = sizeof(HANDLE) < 8 ? PAPERAIRPLANE_NET32_GUID : PAPERAIRPLANE_NET64_GUID;
	if (::WSCInstallProvider(&ProviderChainGuid, pwszPathName, OriginalProtocolInfo, nArrayCount, &nError) == SOCKET_ERROR) {
		return FALSE;
	}
	// 重新排序Winsock目录，将我们的协议链提前 
	// 重新枚举安装的协议 
	FreeLayeredServiceProvider(pProtoInfo);
	pProtoInfo = LoadLayeredServiceProvider(&nProtocols);
	DWORD dwIds[MAXBYTE];
	int nIndex = 0;
	// 添加我们的协议链 
	for (int i = 0; i < nProtocols; i++) {
		if ((pProtoInfo[i].ProtocolChain.ChainLen > 1) &&
			(pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId))
			dwIds[nIndex++] = pProtoInfo[i].dwCatalogEntryId;
	}
	// 添加其它协议 
	for (int i = 0; i < nProtocols; i++) {
		if ((pProtoInfo[i].ProtocolChain.ChainLen <= 1) ||
			(pProtoInfo[i].ProtocolChain.ChainEntries[0] != dwLayeredCatalogId))
			dwIds[nIndex++] = pProtoInfo[i].dwCatalogEntryId;
	}
	// 重新排序Winsock目录 
	if ((nError = ::WSCWriteProviderOrder(dwIds, nIndex)) != ERROR_SUCCESS) {
		return FALSE;
	}
	FreeLayeredServiceProvider(pProtoInfo);

	DWORD dwCategory = LSP_SYSTEM | LSP_INSPECTOR | LSP_REDIRECTOR | LSP_PROXY | LSP_FIREWALL |
		LSP_INBOUND_MODIFY | LSP_OUTBOUND_MODIFY | LSP_CRYPTO_COMPRESS | LSP_LOCAL_CACHE;
	if (NO_ERROR != WSCSetProviderInfo(&PAPERAIRPLANE_MAIN_GUID, ProviderInfoLspCategories, (PBYTE)&dwCategory,
		sizeof(DWORD), 0, &nError)) {
		return FALSE;
	}
	return TRUE;
}

BOOL UninstallLayeredServiceProvider()
{
	LPWSAPROTOCOL_INFOW pProtoInfo;
	int nProtocols;
	DWORD dwLayeredCatalogId;
	// 根据Guid取得分层协议的目录ID号 
	pProtoInfo = LoadLayeredServiceProvider(&nProtocols);
	int nError, i;
	for (i = 0; i < nProtocols; i++) {
		if (memcmp(&PAPERAIRPLANE_MAIN_GUID, &pProtoInfo[i].ProviderId, sizeof(PAPERAIRPLANE_MAIN_GUID)) == 0) {
			dwLayeredCatalogId = pProtoInfo[i].dwCatalogEntryId;
			break;
		}
	}
	if (i < nProtocols) {
		// 移除协议链 
		for (int i = 0; i < nProtocols; i++) {
			if ((pProtoInfo[i].ProtocolChain.ChainLen > 1) &&
				(pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId)) {
				::WSCDeinstallProvider(&pProtoInfo[i].ProviderId, &nError);
			}
		}
		// 移除分层协议 
		::WSCDeinstallProvider(&PAPERAIRPLANE_MAIN_GUID, &nError);
	}
	return TRUE;
}