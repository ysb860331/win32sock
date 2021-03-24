///////////////////////////////////////////////////////
// OverlappedServer.cpp文件

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "../../CommLib/common/initsock.h"

#include <Mswsock.h>
#include <stdio.h>
#include <windows.h>

CInitSock theSock;

#define BUFFER_SIZE 1024

typedef struct _SOCKET_OBJ
{
	SOCKET s;
	int nOutstandingOps;
	LPFN_ACCEPTEX lpfnAcceptEx;
}SOCKET_OBJ, *PSOCKET_OBJ;

typedef struct _BUFFER_OBJ
{
	PSOCKET_OBJ pSocket;
	OVERLAPPED ol;
	char *buff;
	int nLen;
	int nOpertion;
#define OP_ACCEPT 1
#define OP_WRITE 2
#define OP_READ 3
	SOCKET sAccept;
	_BUFFER_OBJ *pNext;
}BUFFER_OBJ, *PBUFFER_OBJ;

HANDLE g_events[WSA_MAXIMUM_WAIT_EVENTS];
int g_nBufferCount;
PBUFFER_OBJ g_pBufferHead, g_pBufferTail;

PSOCKET_OBJ GetSocketObj(SOCKET s)
{
	PSOCKET_OBJ pSocket = (PSOCKET_OBJ)::GlobalAlloc(GPTR, sizeof(SOCKET_OBJ));
	if (pSocket != NULL)
	{
		pSocket->s = s;
	}
	return pSocket;
}

void FreeSocketObj(PSOCKET_OBJ pSocket)
{
	closesocket(pSocket->s);
	GlobalFree(pSocket);
}

PBUFFER_OBJ GetBufferObj(PSOCKET_OBJ pSocket, int nLen)
{
	if (g_nBufferCount > WSA_MAXIMUM_WAIT_EVENTS - 1)
		return NULL;

	PBUFFER_OBJ pBuffer = (PBUFFER_OBJ)::GlobalAlloc(GPTR, sizeof(BUFFER_OBJ));
	if (!pBuffer)
		return NULL;
	pBuffer->ol.hEvent = ::WSACreateEvent();
	pBuffer->nLen = nLen;
	pBuffer->sAccept = INVALID_SOCKET;
	if (g_pBufferHead == NULL)
	{
		g_pBufferHead = g_pBufferTail = pBuffer;
	}
	else
	{
		g_pBufferTail->pNext = pBuffer;
		g_pBufferTail = pBuffer;
	}
	g_events[++g_nBufferCount] = pBuffer->ol.hEvent;
	return pBuffer;
}

void FreeBufferObj(PBUFFER_OBJ pBuffer)
{
	if (pBuffer == NULL)
	{
		return;
	}
	PBUFFER_OBJ pTest = g_pBufferHead;
	BOOL bFind = FALSE;
	if (pTest == pBuffer)
	{
		pTest->pNext = pBuffer;
		bFind = TRUE;
	}
	else
	{
		while (pTest != NULL && pTest->pNext != pBuffer)
		{
			if (pTest->pNext == pBuffer)
			{
				break;
			}
			pTest = pTest->pNext;
		}
		if (pTest != NULL)
		{
			pTest->pNext = pBuffer->pNext;
			if (pBuffer == g_pBufferTail)
			{
				g_pBufferTail = pTest;
			}
		}
		bFind = TRUE;
	}
	if (bFind)
	{
		g_nBufferCount--;
		::CloseHandle(pBuffer->ol.hEvent);
		GlobalFree(pBuffer->buff);
		GlobalFree(pBuffer);
	}
}

PBUFFER_OBJ FindBufferObj(HANDLE hEvent)
{
	PBUFFER_OBJ pBuffer = g_pBufferHead;
	while (pBuffer != NULL)
	{
		if (pBuffer->ol.hEvent == hEvent)
			break;
		pBuffer = pBuffer->pNext;
	} 
	return pBuffer;
}

void ReBuildArray()
{
	PBUFFER_OBJ pBuffer = g_pBufferHead;
	int i = 1;
	while (pBuffer != NULL)
	{
		g_events[i++] = pBuffer->ol.hEvent;
		pBuffer = pBuffer->pNext;
	}
}

void RemoveBuffer(PBUFFER_OBJ pBuffer)
{
	PSOCKET_OBJ pSocket = pBuffer->pSocket;
	if (pSocket->s != INVALID_SOCKET)
	{
		::closesocket(pSocket->s);
		pSocket->s = INVALID_SOCKET;
	}
	if (pSocket->nOutstandingOps == 0)
	{
		FreeSocketObj(pSocket);
	}
	FreeBufferObj(pBuffer);
}

BOOL PostAccept(PBUFFER_OBJ pBuffer)
{	
	PSOCKET_OBJ pSocket = pBuffer->pSocket;
	if (pSocket->lpfnAcceptEx != NULL)
		return FALSE;

	pBuffer->nOpertion = OP_ACCEPT;
	pSocket->nOutstandingOps++;
	// 投递此重叠I/O  
	DWORD dwBytes;
	pBuffer->sAccept = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	BOOL b = pSocket->lpfnAcceptEx(pSocket->s,
		pBuffer->sAccept,
		pBuffer->buff,
		BUFFER_SIZE - ((sizeof(sockaddr_in) + 16) * 2),
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&dwBytes,
		&pBuffer->ol);
	if (!b)
	{
		if (::WSAGetLastError() != WSA_IO_PENDING)
		{
			return FALSE;
		}
	}
	return TRUE;
}

BOOL PostRecv(PBUFFER_OBJ pBuffer)
{
	pBuffer->nOpertion = OP_WRITE;
	pBuffer->pSocket->nOutstandingOps++;

	// 投递此重叠I/O
	DWORD dwBytes;
	DWORD dwFlags = 0;
	WSABUF buf;
	buf.buf = pBuffer->buff;
	buf.len = pBuffer->nLen;
	if (::WSARecv(pBuffer->pSocket->s, &buf, 1, &dwBytes, &dwFlags, &pBuffer->ol, NULL) != NO_ERROR)
	{
		if (::WSAGetLastError() != WSA_IO_PENDING)
			return FALSE;
	}
	return TRUE;
}

BOOL PostSend(PBUFFER_OBJ pBuffer)
{
	pBuffer->nOpertion = OP_READ;
	pBuffer->pSocket->nOutstandingOps++;

	WSABUF buf;
	buf.buf = pBuffer->buff;
	buf.len = pBuffer->nLen;
	DWORD dwBytes;
	DWORD dwFlags = 0;
	if (::WSASend(pBuffer->pSocket->s, &buf,1, &dwBytes, dwFlags, &pBuffer->ol, NULL) != NO_ERROR)
	{
		if (::WSAGetLastError() != WSA_IO_PENDING)
			return FALSE;
	}
	return TRUE;
}

BOOL HandleIO(PBUFFER_OBJ pBuffer)
{
	PSOCKET_OBJ pSocket = pBuffer->pSocket; // 从BUFFER_OBJ对象中提取SOCKET_OBJ对象指针，为的是方便引用
	pBuffer->pSocket->nOutstandingOps--;
	DWORD dwTrans;
	DWORD dwFlags;
	BOOL bRet = ::WSAGetOverlappedResult(pSocket->s, &pBuffer->ol, &dwTrans, FALSE, &dwFlags);
	if (bRet)
	{
		RemoveBuffer(pBuffer);
		return FALSE;
	}
	switch (pBuffer->nOpertion)
	{
	case OP_ACCEPT:
	{
		PSOCKET_OBJ pClient = GetSocketObj(pBuffer->sAccept);
		PBUFFER_OBJ pSend = GetBufferObj(pClient, BUFFER_SIZE);
		if (pSend == NULL)
		{
			printf(" Too much connections! \n");
			FreeSocketObj(pClient);
			return FALSE;
		}
		ReBuildArray();
		pSend->nLen = dwTrans;
		memcpy(pSend->buff, pBuffer->buff, dwTrans);

		if (!PostSend(pSend))
		{
			FreeSocketObj(pClient);
			FreeBufferObj(pSend);
			return FALSE;
		}
		PostAccept(pBuffer);
	}
	break;
	case OP_READ:
	{
		if (dwTrans > 0)
		{
			// 创建一个缓冲区，以发送数据。这里就使用原来的缓冲区
			PBUFFER_OBJ pSend = pBuffer;
			pSend->nLen = dwTrans;

			// 投递发送I/O（将数据回显给客户）
			PostSend(pSend);
		}
		else
		{
			RemoveBuffer(pBuffer);
			return FALSE;
		}
	}
	case OP_WRITE:      // 发送数据完成
	{
		if (dwTrans > 0)
		{
			// 继续使用这个缓冲区投递接收数据的请求
			pBuffer->nLen = BUFFER_SIZE;
			PostRecv(pBuffer);
		}
		else    // 套节字关闭
		{
			// 同样，要先关闭套节字
			if (pSocket->s != INVALID_SOCKET)
			{
				::closesocket(pSocket->s);
				pSocket->s = INVALID_SOCKET;
			}

			if (pSocket->nOutstandingOps == 0)
				FreeSocketObj(pSocket);

			FreeBufferObj(pBuffer);
			return FALSE;
		}
	}
	break;
	default:
		break;
	}
	return TRUE;
}

int main()
{
	int nPort = 4567;
	SOCKET sListen = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN si;
	si.sin_family = AF_INET;
	si.sin_port = ::ntohs(nPort);
	si.sin_addr.S_un.S_addr = INADDR_ANY;
	::bind(sListen, (sockaddr*)&si, sizeof(si));
	::listen(sListen, 200);

	PSOCKET_OBJ pListen = GetSocketObj(sListen);
	DWORD dwBytes;
	GUID GuidAcceptex = WSAID_ACCEPTEX;
	WSAIoctl(pListen->s,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptex,
		sizeof(GuidAcceptex),
		&pListen->lpfnAcceptEx,
		sizeof(pListen->lpfnAcceptEx),
		&dwBytes,
		NULL,
		NULL);

	g_events[0] = ::WSACreateEvent();
	for (int i = 0; i < 5; i ++)
	{
		PostAccept(GetBufferObj(pListen, BUFFER_SIZE));
	}
	::WSASetEvent(g_events[0]);

	while (TRUE)
	{
		int nIndex = ::WSAWaitForMultipleEvents(g_nBufferCount + 1, g_events, FALSE, WSA_INFINITE, FALSE);
		if (nIndex == WSA_WAIT_FAILED)
		{
			printf("WSAWaitForMultipleEvents() Failed\n");
			break;
		}
		nIndex = nIndex - WSA_WAIT_EVENT_0;
		for (int i = nIndex; i <= g_nBufferCount; i ++)
		{
			int nRet = WSAWaitForMultipleEvents(1, &g_events[i], TRUE, 0, FALSE);
			if (nRet == WSA_WAIT_TIMEOUT)
			{
				continue;
			}
			else
			{
				::WSAResetEvent(g_events[i]);
				if (i == 0)
				{
					ReBuildArray();
					break;
				}
				PBUFFER_OBJ pBuffer = FindBufferObj(g_events[i]);
				if (!HandleIO(pBuffer))
				{
					ReBuildArray();
				}
			}
		}
	}
	system("pause");
	return 0;
}