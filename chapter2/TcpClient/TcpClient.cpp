//////////////////////////////////////////////////////////
// TCPClient.cpp文件

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "../../CommLib/common/initsock.h"
#include <stdio.h>
#include <string>
#include <windows.h>

CInitSock initSock;     // 初始化Winsock库


void SendString(SOCKET s, int nIndex)
{
	char szTextSend[100] = { 0 };
	sprintf_s(szTextSend, "other string %d\n", nIndex);
	printf(szTextSend);
	::send(s, szTextSend, strlen(szTextSend), 0);
}

static char operMode = 'n';

DWORD WINAPI ClientThread(LPVOID lpParam)
{
	printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
	printf("\n");
	printf("【r】 表示接收数据 \n");
	printf("【s】 表示发送数据 \n");
	printf("【c】 表示关闭套接字 \n");
	printf("【n】 表示套接字空闲 \n");

	printf("\n\n默认操作方式：n \n");
	printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
	printf("\n");
	operMode = 'n';

	while (true)
	{
		operMode = getchar();
		if (operMode != 'c' && operMode != 's' && operMode != 'r')
		{
			getchar();
			printf("请输入正确操作方式：\n");
			continue;
		}
		getchar();
		printf("模式=%c\n", operMode);
	}
	return 0;
}

int main()
{
	::CreateThread(NULL, 0, ClientThread, NULL, 0, 0);


	// 创建套节字
	SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
	{
		printf(" Failed socket() \n");
		return 0;
	}

	// 也可以在这里调用bind函数绑定一个本地地址
	// 否则系统将会自动安排

	// 填写远程地址信息
	sockaddr_in servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(4567);
	// 注意，这里要填写服务器程序（TCPServer程序）所在机器的IP地址
	// 如果你的计算机没有联网，直接使用127.0.0.1即可
	servAddr.sin_addr.S_un.S_addr = inet_addr("192.168.1.32");

	sockaddr_in guest;
	int guest_len = sizeof(guest);
	::getsockname(s, (struct sockaddr *)&guest, &guest_len);
	printf("before connect getsockname 本地 (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);
	if (::connect(s, (sockaddr*)&servAddr, sizeof(servAddr)) == -1)
	{
		printf(" Failed connect() \n");
		return 0;
	}

	//sockaddr_in guest;
	guest_len = sizeof(guest);
	::getsockname(s, (struct sockaddr *)&guest, &guest_len);
	printf("getsockname 本地 (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);

// 	char szTextSend[100] = { 0 };
// 	sprintf_s(szTextSend, "ip: (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);
// 	::send(s, szTextSend, strlen(szTextSend), 0);

	//sockaddr_in guest;
	guest_len = sizeof(guest);
	::getpeername(s, (struct sockaddr *)&guest, &guest_len);
	printf("getpeername 外地 (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);

// 	int nDataIndex = 0;
// 	while (true)
// 	{
// 		sprintf_s(szTextSend, "测试数据 %5d\n", nDataIndex++);
// 		::send(s, szTextSend, strlen(szTextSend), 0);
// 		::Sleep(1000);
// 	}

	int nSendIndex = 0;
	while (true)
	{
		if (operMode == 'c')
		{
			::closesocket(s);
		}
		else if (operMode == 'r')
		{
			// 接收数据
			int nSendIndex = 0;
			char buff[256];
			int nRecv = ::recv(s, buff, 256, 0);
			if (nRecv > 0)
			{
				if (nRecv >= 256)
					nRecv = 255;
				buff[nRecv] = '\0';
				printf("接收到数据：%s\n", buff);
			}	
		}
		else if (operMode == 's')
		{
			SendString(s, nSendIndex);
			nSendIndex++;
		}
		if (operMode != 'n')
		{
			operMode = 'n';
		}
	}
	system("pause");
	// 关闭套节字
	//::closesocket(s);
	return 0;
}