/////////////////////////////////////////////////////
// EventSelectServer.h文件

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "../../CommLib/common/initsock.h"

#include <stdio.h>
#include <windows.h>

DWORD WINAPI ServerThread(LPVOID lpParam);      

typedef struct _SOCKET_OBJ 
{
	SOCKET s;
	HANDLE event;
	sockaddr_in si;

	_SOCKET_OBJ *pNext;
}SOCKET_OBJ, *PSOCKET_OBJ;

typedef struct _THREAD_OBJ
{
	CRITICAL_SECTION cs;
	HANDLE events[WSA_MAXIMUM_WAIT_EVENTS];
	int nSocketCount;
	PSOCKET_OBJ pSocketHead;
	PSOCKET_OBJ pSocketTail;

	_THREAD_OBJ *pNext;
	
}THREAD_OBJ, *PTHREAD_OBJ;

PTHREAD_OBJ g_pThreadList;
CRITICAL_SECTION g_cs;

LONG g_nTotalConnections;
LONG g_nCurConnections;

PSOCKET_OBJ GetSocketObj(SOCKET s)
{
	PSOCKET_OBJ pSocket = (PSOCKET_OBJ)GlobalAlloc(GPTR, sizeof(SOCKET_OBJ));
	if (pSocket != NULL)
	{
		pSocket->event = ::WSACreateEvent();
		pSocket->s = s;
	}
	return pSocket;
}

void FreeSocketObj(PSOCKET_OBJ pSocket)
{
	if (pSocket == NULL)
	{
		return;
	}
	::CloseHandle(pSocket->event);
	if (pSocket->s != INVALID_SOCKET)
	{
		closesocket(pSocket->s);
	}
	::GlobalFree(pSocket);
}

PTHREAD_OBJ GetThreadObj()
{
	PTHREAD_OBJ pThread = (PTHREAD_OBJ)::GlobalAlloc(GPTR, sizeof(THREAD_OBJ));
	if (pThread != NULL)
	{
		InitializeCriticalSection(&pThread->cs);
		pThread->events[0] = ::WSACreateEvent();

		::EnterCriticalSection(&g_cs);
		pThread->pNext = g_pThreadList;
		g_pThreadList = pThread;
		::LeaveCriticalSection(&g_cs);
	}

	return pThread;
}

void FreeThreadObj(PTHREAD_OBJ pThread)
{
	::EnterCriticalSection(&g_cs);
	PTHREAD_OBJ p = g_pThreadList;
	if (p == pThread)
	{
		g_pThreadList = p->pNext;
	}
	else
	{
		while (p != NULL && p->pNext != pThread)
		{
			p = p->pNext;
		}
		if (p != NULL)
		{
			p->pNext = pThread->pNext;
		}
	}
	::LeaveCriticalSection(&g_cs);

	CloseHandle(pThread->events[0]);
	DeleteCriticalSection(&pThread->cs);
	::GlobalFree(pThread);
}

BOOL InsertSocketObj(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{
	BOOL bRet = FALSE;
	EnterCriticalSection(&pThread->cs);
	if (pThread->nSocketCount < WSA_MAXIMUM_WAIT_EVENTS -1)
	{
		PSOCKET_OBJ p = pThread->pSocketHead;
		if (p == NULL)
		{
			pThread->pSocketHead = pThread->pSocketTail = pSocket;
		}
		else
		{
			pThread->pSocketTail->pNext = pSocket;
			pThread->pSocketTail = pSocket;
		}
		pThread->nSocketCount++;
		bRet = TRUE;
	}
	LeaveCriticalSection(&pThread->cs);
	if (bRet)
	{
		::InterlockedIncrement(&g_nTotalConnections);
		::InterlockedIncrement(&g_nCurConnections);
	}
	return bRet;
}

void AssignToFreeThread(PSOCKET_OBJ pSocket)
{
	pSocket->pNext = NULL;

	EnterCriticalSection(&g_cs);
	PTHREAD_OBJ pThread = g_pThreadList;
	while (pThread)
	{
		if (InsertSocketObj(pThread, pSocket))
			break;
		pThread = pThread->pNext;
	}
	if (pThread == NULL)
	{		
		pThread = GetThreadObj();
		InsertSocketObj(pThread, pSocket);
		CreateThread(NULL, 0, ServerThread, pThread, 0, NULL);
	}
	::LeaveCriticalSection(&g_cs);

	::WSASetEvent(pThread->events[0]);
}

void RemoveSocketObj(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{
	EnterCriticalSection(&pThread->cs);
	PSOCKET_OBJ pTest = pThread->pSocketHead;
	if (pTest == pSocket)
	{
		if (pThread->pSocketHead == pThread->pSocketTail)
		{
			pThread->pSocketHead = pThread->pSocketTail = pTest->pNext;
		}
		else
		{
			pThread->pSocketHead = pSocket->pNext;
		}
	}
	else
	{
		while (pTest != NULL && pTest->pNext != pSocket)
		{
			pTest = pTest->pNext;
		}
		if (pTest != NULL)
		{
			if (pSocket == pThread->pSocketTail)
			{
				pThread->pSocketTail = pTest;
			}
			pTest->pNext = pSocket->pNext;
		}
	}
	pThread->nSocketCount--;
	LeaveCriticalSection(&pThread->cs);

	InterlockedDecrement(&g_nCurConnections);
	::WSASetEvent(pThread->events[0]);
}

PSOCKET_OBJ FindSocketObj(PTHREAD_OBJ pThread, int nIndex)
{
	PSOCKET_OBJ p = pThread->pSocketHead;
	while (--nIndex)
	{
		if (p == NULL)
		{
			return NULL;
		}
		p = p->pNext;
	}
	return p;
}

void ReBuildArray(PTHREAD_OBJ pThread)
{
	EnterCriticalSection(&pThread->cs);
	int n = 1;
	PSOCKET_OBJ pSocket = pThread->pSocketHead;
	while (true)
	{
		if (pSocket == NULL)
		{
			break;
		}
		pThread->events[n++] = pSocket->event;
		pSocket = pSocket->pNext;
	}
	LeaveCriticalSection(&pThread->cs);
}

BOOL HandleIO(PTHREAD_OBJ pThread, PSOCKET_OBJ pSocket)
{
	WSANETWORKEVENTS event;
	WSAEnumNetworkEvents(pSocket->s, pSocket->event, &event);
	do 
	{
		if (event.lNetworkEvents & FD_READ)
		{
			if (event.iErrorCode[FD_READ_BIT] == 0)
			{
				char szText[256];
				int nRecv = ::recv(pSocket->s, szText, strlen(szText), 0);
				if (nRecv > 0)
				{
					szText[nRecv] = '\0';
					printf("接收到数据：%s \n", szText);
				}
			}
			else
			{
				break;
			}
		}
		else if (event.lNetworkEvents & FD_CLOSE)
		{
			break;
		}
		else if (event.lNetworkEvents & FD_WRITE)
		{
			continue;
		}
		return TRUE;
	} while (FALSE);

	RemoveSocketObj(pThread, pSocket);
	FreeSocketObj(pSocket);
	return FALSE;
}

DWORD WINAPI ServerThread(LPVOID lpParam)
{
	PTHREAD_OBJ pThreadObj = (PTHREAD_OBJ)lpParam;
	while (true)
	{
		int nIndex = ::WSAWaitForMultipleEvents(pThreadObj->nSocketCount + 1, pThreadObj->events, FALSE, WSA_INFINITE, FALSE);
		nIndex = nIndex - WSA_WAIT_EVENT_0;
		for (int i = nIndex; i < pThreadObj->nSocketCount + 1; i++)
		{
			nIndex = ::WSAWaitForMultipleEvents(1, &pThreadObj->events[i], FALSE, 1000, FALSE);
			if (nIndex == WSA_WAIT_FAILED || nIndex == WSA_WAIT_TIMEOUT)
			{
				continue;
			}
			else
			{
				if (i == 0)
				{
					ReBuildArray(pThreadObj);
					if (pThreadObj->nSocketCount == 0)
					{
						FreeThreadObj(pThreadObj);
						return 0;
					}
					::WSAResetEvent(pThreadObj->events[0]);
				}
				else
				{
					PSOCKET_OBJ pSocket = FindSocketObj(pThreadObj, i);
					if (pSocket)
					{
						if (!HandleIO(pThreadObj, pSocket))
						{
							ReBuildArray(pThreadObj);
						}
					}
					else
					{
						printf("no find socket: %5d\n", i);
					}
				}
			}
		}
	}
	return 0;
}