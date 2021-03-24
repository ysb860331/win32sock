//////////////////////////////////////////////////////////
// TCPClient.cpp�ļ�

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "../../CommLib/common/initsock.h"
#include <stdio.h>
#include <string>
#include <windows.h>

CInitSock initSock;     // ��ʼ��Winsock��


void SendString(SOCKET s, int nIndex)
{
	char szTextSend[100] = { 0 };
	sprintf_s(szTextSend, "other string %d\n", nIndex);
	::send(s, szTextSend, strlen(szTextSend), 0);
}

static char operMode = 'n';

DWORD WINAPI ClientThread(LPVOID lpParam)
{
	printf("�����������ʽ��\n");
	printf("��r�� ��ʾ�������� \n");
	printf("��s�� ��ʾ�������� \n");
	printf("��c�� ��ʾ�ر��׽��� \n");

	printf("\n\nĬ�ϲ�����ʽ��r \n");
	operMode = 'r';

	while (true)
	{
		operMode = getchar();
		if (operMode != 'c' && operMode != 's' && operMode != 'r')
		{
			getchar();
			printf("��������ȷ������ʽ��\n");
			continue;
		}
		getchar();
		printf("ģʽ=%c\n", operMode);
	}
	return 0;
}

int main()
{
	::CreateThread(NULL, 0, ClientThread, NULL, 0, 0);


	// �����׽���
	SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
	{
		printf(" Failed socket() \n");
		return 0;
	}

	// Ҳ�������������bind������һ�����ص�ַ
	// ����ϵͳ�����Զ�����

	// ��дԶ�̵�ַ��Ϣ
	sockaddr_in servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(4567);
	// ע�⣬����Ҫ��д����������TCPServer�������ڻ�����IP��ַ
	// �����ļ����û��������ֱ��ʹ��127.0.0.1����
	servAddr.sin_addr.S_un.S_addr = inet_addr("192.168.1.32");

	sockaddr_in guest;
	int guest_len = sizeof(guest);
	::getsockname(s, (struct sockaddr *)&guest, &guest_len);
	printf("before connect getsockname ���� (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);
	if (::connect(s, (sockaddr*)&servAddr, sizeof(servAddr)) == -1)
	{
		printf(" Failed connect() \n");
		return 0;
	}

	//sockaddr_in guest;
	guest_len = sizeof(guest);
	::getsockname(s, (struct sockaddr *)&guest, &guest_len);
	printf("getsockname ���� (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);

	char szTextSend[100] = { 0 };
	sprintf_s(szTextSend, "ip: (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);
	::send(s, szTextSend, strlen(szTextSend), 0);

	//sockaddr_in guest;
	guest_len = sizeof(guest);
	::getpeername(s, (struct sockaddr *)&guest, &guest_len);
	printf("getpeername ��� (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);

// 	int nDataIndex = 0;
// 	while (true)
// 	{
// 		sprintf_s(szTextSend, "�������� %5d\n", nDataIndex++);
// 		::send(s, szTextSend, strlen(szTextSend), 0);
// 		::Sleep(1000);
// 	}

	while (true)
	{
		if (operMode == 'c')
		{
			::closesocket(s);
			operMode = 'n';
		}
		else if (operMode == 'r')
		{
			// ��������
			int nSendIndex = 0;
			char buff[256];
			int nRecv = ::recv(s, buff, 256, 0);
			if (nRecv > 0)
			{
				if (nRecv >= 256)
					nRecv = 255;
				buff[nRecv] = '\0';
				printf(" ���յ����ݣ�%s\n", buff);
			}	
			operMode = 'n';
		}
	}
	system("pause");
	// �ر��׽���
	//::closesocket(s);
	return 0;
}