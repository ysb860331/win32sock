//////////////////////////////////////////////////
// TCPServer.cpp�ļ�


#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "../../CommLib/common/initsock.h"
#include <stdio.h>
//# include <stdlib.h>
CInitSock initSock;     // ��ʼ��Winsock��

int main()
{
	// �����׽���
	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListen == INVALID_SOCKET)
	{
		printf("Failed socket() \n");
		return 0;
	}

	// ���sockaddr_in�ṹ
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(4567);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;

	// ������׽��ֵ�һ�����ص�ַ
	if (::bind(sListen, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf("Failed bind() \n");
		return 0;
	}


	sockaddr_in guest;
	int guest_len = sizeof(guest);


	// �������ģʽ
	if (::listen(sListen, 2) == SOCKET_ERROR)
	{
		printf("Failed listen() \n");
		return 0;
	}

	// ѭ�����ܿͻ�����������
	sockaddr_in remoteAddr;
	int nAddrLen = sizeof(remoteAddr);
	SOCKET sClient;
	char szText[] = " TCP Server Demo! \r\n";
	int i = 0;
	while (TRUE)
	{
		// ����һ��������
		sClient = ::accept(sListen, (SOCKADDR*)&remoteAddr, &nAddrLen);
		if (sClient == INVALID_SOCKET)
		{
			printf("Failed accept()");
			continue;
		}

		int a = ::getsockname(sClient, (struct sockaddr *)&guest, &guest_len);

		printf(" ���ܵ�һ�����ӡ�%d����%s  port:(%d) \r\n", i, inet_ntoa(remoteAddr.sin_addr), guest.sin_port);

		// ��ͻ��˷�������

		sprintf_s(szText, "��ţ�%d \r\n", i++);
		::send(sClient, szText, strlen(szText), 0);
		// �ر�ͬ�ͻ��˵�����
		::closesocket(sClient);
	}

	// �رռ����׽���
	::closesocket(sListen);
	system("pause");
	return 0;
}