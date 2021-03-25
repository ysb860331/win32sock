#include "iocp.h"
#pragma comment(lib, "WS2_32.lib")

CIOCPServer::CIOCPServer()
{
	// 列表
	m_pFreeBufferList = NULL;
	m_pFreeContextList = NULL;
	m_pPendingAccepts = NULL;
	m_pConnectionList = NULL;

	m_nFreeBufferCount = 0;
	m_nFreeContextCount = 0;
	m_nPendingAcceptCount = 0;
	m_nCurrentConnection = 0;

	::InitializeCriticalSection(&m_FreeBufferListLock);
	::InitializeCriticalSection(&m_FreeContextListLock);
	::InitializeCriticalSection(&m_PendingAcceptsLock);
	::InitializeCriticalSection(&m_ConnectionListLock);

	m_hAcceptEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hRepostEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	m_nRepostCount = 0;

	m_nPort = 4567;

	m_nInitialAccepts = 3;
	m_nInitialReads = 4;
	m_nMaxAccepts = 100;
	m_nMaxSends = 20;
	m_nMaxFreeBuffers = 200;
	m_nMaxFreeContexts = 100;
	m_nMaxConnections = 2000;

	m_hListenThread = NULL;
	m_hCompletion = NULL;
	m_sListen = INVALID_SOCKET;
	m_lpfnAcceptEx = NULL;
	m_lpfnGetAcceptExSockaddrs = NULL;

	m_bShutDown = FALSE;
	m_bServerStarted = FALSE;

	// 初始化WS2_32.dll
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	::WSAStartup(sockVersion, &wsaData);
}


CIOCPServer::~CIOCPServer()
{
	ShutDown();
	if (m_sListen != INVALID_SOCKET)
	{
		::closesocket(m_sListen);
	}
	if (m_hListenThread != NULL)
	{
		::CloseHandle(m_hListenThread);
	}
	::CloseHandle(m_hAcceptEvent);
	::CloseHandle(m_hRepostEvent);

	::DeleteCriticalSection(&m_FreeBufferListLock);
	::DeleteCriticalSection(&m_FreeContextListLock);
	::DeleteCriticalSection(&m_PendingAcceptsLock);
	::DeleteCriticalSection(&m_ConnectionListLock);

	::WSACleanup();
}

void CIOCPServer::ShutDown()
{
	if (!m_bServerStarted)
	{
		return;
	}

	m_bShutDown = TRUE;
	::SetEvent(m_hAcceptEvent);
	::WaitForSingleObject(m_hListenThread, INFINITE);
	::CloseHandle(m_hListenThread);
	m_hListenThread = NULL;
	m_bServerStarted = FALSE;
}

void CIOCPServer::CloseAConnection(CIOCPContext *pContext)
{
	::EnterCriticalSection(&m_ConnectionListLock);
	CIOCPContext *pTest = m_pConnectionList;
	if (pTest == pContext)
	{
		m_pConnectionList = pTest->pNext;
		m_nCurrentConnection--;
	}
	else
	{
		while (pTest != NULL && pTest->pNext != pContext)
		{
			pTest = pTest->pNext;
		}
		if (pTest != NULL)
		{
			pTest->pNext = pContext->pNext;
			m_nCurrentConnection--;
		}
	}
	::LeaveCriticalSection(&m_ConnectionListLock);

	::EnterCriticalSection(&pContext->Lock);
	if (pContext->s != INVALID_SOCKET)
	{
		::closesocket(pContext->s);
		pContext->s = INVALID_SOCKET;
	}
	pContext->bClosing = TRUE;
	::LeaveCriticalSection(&pContext->Lock);
}

void CIOCPServer::CloseAllConnections()
{
	::EnterCriticalSection(&m_ConnectionListLock);
	CIOCPContext *pContext = m_pConnectionList;
	while (pContext != NULL)
	{
		::EnterCriticalSection(&pContext->Lock);
		if (pContext->s != INVALID_SOCKET)
		{
			::closesocket(pContext->s);
			pContext->s = INVALID_SOCKET;
		}
		pContext->bClosing = TRUE;
		::LeaveCriticalSection(&pContext->Lock);
		pContext = pContext->pNext;
	}
	m_pConnectionList = NULL;
	m_nCurrentConnection = 0;
	::LeaveCriticalSection(&m_ConnectionListLock);
}

CIOCPBuffer *CIOCPServer::AllocateBuffer(int nLen)
{
	CIOCPBuffer *pBuffer = NULL;
	if (nLen > BUFFER_SIZE)
	{
		return NULL;
	}
	EnterCriticalSection(&m_FreeBufferListLock);
	if (m_pFreeBufferList == NULL)
	{
		pBuffer = (CIOCPBuffer*)::HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CIOCPBuffer) + BUFFER_SIZE);
	}
	else
	{
		pBuffer = m_pFreeBufferList;
		m_pFreeBufferList = m_pFreeBufferList->pNext;
		pBuffer->pNext = NULL;
		m_nFreeBufferCount--;
	}
	LeaveCriticalSection(&m_FreeBufferListLock);

	if (pBuffer != NULL)
	{
		pBuffer->buff = (char*)(pBuffer + 1);
		pBuffer->nLen = nLen;
	}
	return pBuffer;
}

void CIOCPServer::ReleaseBuffer(CIOCPBuffer *pBuffer)
{
	::EnterCriticalSection(&m_FreeBufferListLock);
	if (m_nFreeBufferCount <= m_nMaxFreeContexts)
	{
		memset(pBuffer, 0, sizeof(CIOCPBuffer) + BUFFER_SIZE);
		pBuffer->pNext = m_pFreeBufferList;
		m_pFreeBufferList = pBuffer;
		m_nFreeBufferCount++;
	}
	else
	{
		::HeapFree(::GetProcessHeap(), 0, pBuffer);
	}
	::LeaveCriticalSection(&m_FreeBufferListLock);
}

CIOCPContext *CIOCPServer::AllocateContext(SOCKET s)
{
	CIOCPContext *pContext = NULL;
	::EnterCriticalSection(&m_FreeContextListLock);
	if (m_pFreeContextList == NULL)
	{
		pContext = (CIOCPContext *)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CIOCPContext));
		::InitializeCriticalSection(&pContext->Lock);
	}
	else
	{
		pContext = m_pFreeContextList;
		m_pFreeContextList = m_pFreeContextList->pNext;
		pContext->pNext = NULL;

		m_nFreeContextCount--;
	}
	::LeaveCriticalSection(&m_FreeContextListLock);

	if (pContext != NULL)
	{
		pContext->s = s;
	}
	return pContext;
}

void CIOCPServer::ReleaseContext(CIOCPContext *pContext)
{
	if (pContext->s != INVALID_SOCKET)
	{
		::closesocket(pContext->s);
	}
	CIOCPBuffer *pNext;
	while (pContext->pOutOfOrderReads != NULL)
	{
		pNext = pContext->pOutOfOrderReads->pNext;
		ReleaseBuffer(pContext->pOutOfOrderReads);
		pContext->pOutOfOrderReads = pNext;
	}

	::EnterCriticalSection(&m_FreeContextListLock);
	if (m_nFreeContextCount <= m_nMaxFreeContexts)
	{
		CRITICAL_SECTION cstmp = pContext->Lock;
		memset(pContext, 0, sizeof(CIOCPContext));
		pContext->Lock = cstmp;
		pContext->pNext = m_pFreeContextList;
		m_pFreeContextList = pContext;

		m_nMaxFreeContexts++;
	}
	else
	{
		::DeleteCriticalSection(&pContext->Lock);
		::HeapFree(::GetProcessHeap(), 0, pContext);
	}
	::LeaveCriticalSection(&m_FreeContextListLock);
}

void CIOCPServer::FreeBuffers()
{
	::EnterCriticalSection(&m_FreeBufferListLock);
	CIOCPBuffer *pFreeBuffer = m_pFreeBufferList;
	CIOCPBuffer *pNextBuffer;
	while (pFreeBuffer != NULL)
	{
		pNextBuffer = pFreeBuffer->pNext;
		if (!::HeapFree(::GetProcessHeap(), 0, pFreeBuffer))
		{
#ifdef _DEBUG
			::OutputDebugString("  FreeBuffers释放内存出错！");
#endif // _DEBUG
			break;
		}
		pFreeBuffer = pNextBuffer;
	}
	m_pFreeBufferList = NULL;
	m_nFreeContextCount = 0;
	::LeaveCriticalSection(&m_FreeBufferListLock);
}

void CIOCPServer::FreeContexts()
{
	::EnterCriticalSection(&m_FreeContextListLock);
	CIOCPContext* pContext = m_pFreeContextList;
	CIOCPContext *pNext;
	while (pContext != NULL)
	{
		pNext = pContext->pNext;
		::DeleteCriticalSection(&pContext->Lock);

		if (!::HeapFree(::GetProcessHeap(), 0, pContext))
		{
#ifdef _DEBUG
			::OutputDebugString("  FreeBuffers释放内存出错！");
#endif // _DEBUG
			break;
		}
		pContext = pNext;
	}
	m_pFreeContextList = NULL;
	m_nFreeContextCount = 0;
	::LeaveCriticalSection(&m_FreeContextListLock);
}

BOOL CIOCPServer::AddAConnection(CIOCPContext *pContext)
{
	::EnterCriticalSection(&m_ConnectionListLock);
	if (m_nCurrentConnection <= m_nMaxConnections)
	{
		pContext->pNext = m_pConnectionList;
		m_pConnectionList = pContext;
		m_nMaxConnections++;
		LeaveCriticalSection(&m_ConnectionListLock);
		return TRUE;
	}
	::LeaveCriticalSection(&m_ConnectionListLock);
	return FALSE;
}

BOOL CIOCPServer::InsertPendingAccept(CIOCPBuffer *pBuffer)
{
	::EnterCriticalSection(&m_PendingAcceptsLock);
	if (m_pPendingAccepts == NULL)
	{
		m_pPendingAccepts = pBuffer;
	}
	else
	{
		pBuffer->pNext = m_pPendingAccepts;
		m_pPendingAccepts = pBuffer;
	}
	m_nPendingAcceptCount++;
	::LeaveCriticalSection(&m_PendingAcceptsLock);
	return TRUE;
}

BOOL CIOCPServer::RemovePendingAccept(CIOCPBuffer *pBuffer)
{
	BOOL bResult = FALSE;
	::EnterCriticalSection(&m_PendingAcceptsLock);
	CIOCPBuffer *pTest = m_pPendingAccepts;
	if (pTest == m_pPendingAccepts)
	{
		m_pPendingAccepts = pBuffer;
		bResult = TRUE;
	}
	else
	{
		while (pTest != NULL && pTest->pNext != pBuffer)
		{
			pTest = pTest->pNext;
		}
		if (pTest != NULL)
		{
			pTest->pNext = pBuffer->pNext;
			bResult = TRUE;
		}
	}
	if (bResult)
	{
		m_nPendingAcceptCount--;
	}
	::LeaveCriticalSection(&m_PendingAcceptsLock);
	return TRUE;
}

CIOCPBuffer *CIOCPServer::GetNextReadBuffer(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	return NULL;
}

BOOL CIOCPServer::PostAccept(CIOCPBuffer *pBuffer)
{
	return TRUE;
}

BOOL CIOCPServer::PostSend(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	return TRUE;
}

BOOL CIOCPServer::PostRecv(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{
	return TRUE;
}

void CIOCPServer::HandleIO(DWORD dwKey, CIOCPBuffer *pBuffer, DWORD dwTrans, int nError)
{

}

BOOL CIOCPServer::Start(int nPort /* = 4567 */, int nMaxConnections /* = 2000 */, int nMaxFreeBuffers /* = 200 */, int nMaxFreeContexts /* = 100 */, int nInitialReads /* = 4 */)
{
	if (m_bServerStarted)
		return FALSE;
	m_nPort = nPort;
	m_nMaxConnections = nMaxConnections;
	m_nMaxFreeBuffers = nMaxFreeBuffers;
	m_nMaxFreeContexts = nMaxFreeContexts;
	m_nInitialReads = nInitialReads;

	m_bShutDown = FALSE;
	m_bServerStarted = TRUE;

	m_sListen = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN si;
	si.sin_family = AF_INET;
	si.sin_port = ::ntohs(m_nPort);
	si.sin_addr.S_un.S_addr = INADDR_ANY;

	if (::bind(m_sListen, (sockaddr*)&si, sizeof(si)) == SOCKET_ERROR)
	{
		m_bServerStarted = FALSE;
		return FALSE;
	}
	::listen(m_sListen, 3);

	m_hCompletion = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	DWORD dwBytes;
	::WSAIoctl(m_sListen,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx,
		sizeof(GuidAcceptEx),
		&m_lpfnAcceptEx,
		sizeof(m_lpfnAcceptEx),
		&dwBytes,
		NULL,
		NULL);
	GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
	::WSAIoctl(m_sListen,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidGetAcceptExSockaddrs,
		sizeof(GuidGetAcceptExSockaddrs),
		&m_lpfnGetAcceptExSockaddrs,
		sizeof(m_lpfnGetAcceptExSockaddrs),
		&dwBytes,
		NULL,
		NULL);
	::CreateIoCompletionPort((HANDLE)m_sListen, m_hCompletion, 0, 0);
	WSAEventSelect(m_sListen, m_hAcceptEvent, FD_ACCEPT);
	m_hListenThread = ::CreateThread(NULL, 0, _ListenThreadProc, this, 0, NULL);
	return TRUE;
}

DWORD WINAPI CIOCPServer::_ListenThreadProc(LPVOID lpParam)
{
	CIOCPServer *pThis = (CIOCPServer*)lpParam;
	CIOCPBuffer *pBuffer;
	for (int i = 0; i < pThis->m_nInitialAccepts; i ++)
	{
		pBuffer = pThis->AllocateBuffer(BUFFER_SIZE);
		if (pBuffer == NULL)
		{
			return -1;
		}
		pThis->InsertPendingAccept(pBuffer);
		pThis->PostAccept(pBuffer);
	}

	return TRUE;
}

BOOL CIOCPServer::SendText(CIOCPContext *pContext, char *pszText, int nLen)
{
	return TRUE;
}

void CIOCPServer::OnConnectionEstablished(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{

}

void CIOCPServer::OnConnectionClosing(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{

}

void CIOCPServer::OnConnectionError(CIOCPContext *pContext, CIOCPBuffer *pBuffer, int nError)
{

}

void CIOCPServer::OnReadCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{

}

void CIOCPServer::OnWriteCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{

}