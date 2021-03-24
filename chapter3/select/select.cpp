//////////////////////////////////////////////////////
// select.cpp�ļ�
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "../../CommLib/common/initsock.h"
#include <stdio.h>

CInitSock theSock;
int main()
{
	USHORT nPort = 4567;

	// ����
	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(nPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;

	// ��
	if (::bind(sListen, (sockaddr *)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf(" Failed bind() \n");
		return -1;
	}

	//����
	::listen(sListen, 5);

	// select ģ�ʹ������
	// 1) ��ʼ���׽��ּ��� fdSocket, ��Ӽ����׽��־�����������
	fd_set fdSocket;
	FD_ZERO(&fdSocket);
	FD_SET(sListen, &fdSocket);

	char szText[] = " TCP Server Demo! \r\n";
	int SocketIndex = 0;
	while (TRUE)
	{
		// 2) ��fdSockete��ϵ�һ������fdRead���ݸ�select������
		// �����¼�������select����һ��fdRead������û��δ��I/O�������׽��־����Ȼ�󷵻�
		fd_set fdRead = fdSocket;
		int nRet = ::select(0, &fdRead, NULL, NULL, NULL);
		if (nRet > 0)
		{
			// 3) ͨ����ԭ����fdSocket������select�������fdRead���ϱȽ�
			// ȷ����Щ�׽�����δ��I/O,����һ��������Щi/o

			for (int i = 0; i < fdSocket.fd_count; i++)
			{
				if (FD_ISSET(fdSocket.fd_array[i], &fdRead))
				{
					if (fdSocket.fd_array[i] == sListen)     // ��1�������׽��ֽ��յ�������
					{
						if (fdSocket.fd_count < FD_SETSIZE)
						{
							sockaddr_in addrRemote;
							int nAddrLen = sizeof(addrRemote);
							SOCKET sNew = ::accept(sListen, (SOCKADDR*)&addrRemote, &nAddrLen);
							FD_SET(sNew, &fdSocket);
							printf("���յ����� (%s)  port:(%d)\n", ::inet_ntoa(addrRemote.sin_addr), addrRemote.sin_port);


							sockaddr_in guest;
							int guest_len = sizeof(guest);
							::getsockname(sNew, (struct sockaddr *)&guest, &guest_len);
							printf("getsockname ���� (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);

							//sockaddr_in guest;
							 guest_len = sizeof(guest);
							::getpeername(sNew, (struct sockaddr *)&guest, &guest_len);
							printf("getpeername ��� (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);

							sprintf_s(szText, "��ţ�%d \r\n", SocketIndex++);
							::send(sNew, szText, strlen(szText), 0);
						}
						else
						{
							printf(" Too much connections! \n");
							continue;
						}
					}
					else
					{
						char szText[256];
						int nRecv = ::recv(fdSocket.fd_array[i], szText, strlen(szText), 0);
						if (nRecv > 0) // ��2���ɶ�
						{
							szText[nRecv] = '\0';
							printf("���յ����ݣ�%s \n", szText);
						}
						else
						{
							sockaddr_in guest;
							int guest_len = sizeof(guest);
							::getpeername(fdSocket.fd_array[i], (struct sockaddr *)&guest, &guest_len);
							printf("�ر�socket (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);

							::closesocket(fdSocket.fd_array[i]);
							FD_CLR(fdSocket.fd_array[i], &fdSocket);
						}
					}
				}
			}
		}
	}
	
	system("pause");
	return 0;
}