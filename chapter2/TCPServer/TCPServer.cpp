//////////////////////////////////////////////////
// TCPServer.cpp文件


#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "../../CommLib/common/initsock.h"
#include <stdio.h>
//# include <stdlib.h>
CInitSock initSock;     // 初始化Winsock库

int main()
{
	// 创建套节字
	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListen == INVALID_SOCKET)
	{
		printf("Failed socket() \n");
		return 0;
	}

	// 填充sockaddr_in结构
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(4567);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;

	// 绑定这个套节字到一个本地地址
	if (::bind(sListen, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf("Failed bind() \n");
		return 0;
	}


	sockaddr_in guest;
	int guest_len = sizeof(guest);


	// 进入监听模式
	if (::listen(sListen, 2) == SOCKET_ERROR)
	{
		printf("Failed listen() \n");
		return 0;
	}

	// 循环接受客户的连接请求
	sockaddr_in remoteAddr;
	int nAddrLen = sizeof(remoteAddr);
	SOCKET sClient;
	char szText[] = " TCP Server Demo! \r\n";
	int i = 0;
	while (TRUE)
	{
		// 接受一个新连接
		sClient = ::accept(sListen, (SOCKADDR*)&remoteAddr, &nAddrLen);
		if (sClient == INVALID_SOCKET)
		{
			printf("Failed accept()");
			continue;
		}

		int a = ::getsockname(sClient, (struct sockaddr *)&guest, &guest_len);

		printf(" 接受到一个连接【%d】：%s  port:(%d) \r\n", i, inet_ntoa(remoteAddr.sin_addr), guest.sin_port);

		// 向客户端发送数据

		sprintf_s(szText, "编号：%d \r\n", i++);
		::send(sClient, szText, strlen(szText), 0);
		// 关闭同客户端的连接
		::closesocket(sClient);
	}

	// 关闭监听套节字
	::closesocket(sListen);
	system("pause");
	return 0;
}