#include "EventServerSelect.h"
//#include <windows.h>

// ³õÊ¼»¯Winsock¿â
CInitSock theSock;

int main()
{
	USHORT uPort = 4567;

	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(uPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;

	if (::bind(sListen, (sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf("bind failed\n");
		return -1;
	}
	listen(sListen, 200);

	HANDLE event = ::WSACreateEvent();
	WSAEventSelect(sListen, event, FD_ACCEPT | FD_CLOSE);

	InitializeCriticalSection(&g_cs);
	while (true)
	{
		int nCode = ::WaitForSingleObject(event, 5000);
		if (nCode == WSA_WAIT_FAILED)
		{
			printf(" Failed WaitForSingleObject() \n");
			break;
		}
		else if (nCode == WSA_WAIT_TIMEOUT)
		{
			printf("\n");
			continue;
		}
		else
		{
			::WSAResetEvent(event);
			while (true)
			{
				sockaddr_in si;
				int nLen = sizeof(si);
				SOCKET sNew = ::accept(sListen, (sockaddr*)&si, &nLen);
				if (sNew == SOCKET_ERROR)
				{
					break;
				}
				PSOCKET_OBJ pSocket = GetSocketObj(sNew);
				pSocket->si = si;
				::WSAEventSelect(pSocket->s, pSocket->event, FD_READ | FD_CLOSE | FD_WRITE);
				AssignToFreeThread(pSocket);
			}
		}
	}
	DeleteCriticalSection(&g_cs);
	return 0;
}