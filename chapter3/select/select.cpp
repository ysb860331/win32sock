//////////////////////////////////////////////////////
// select.cpp文件
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "../../CommLib/common/initsock.h"
#include <stdio.h>

CInitSock theSock;
int main()
{
	USHORT nPort = 4567;

	// 创建
	SOCKET sListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(nPort);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;

	// 绑定
	if (::bind(sListen, (sockaddr *)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		printf(" Failed bind() \n");
		return -1;
	}

	//监听
	::listen(sListen, 5);

	// select 模型处理过程
	// 1) 初始化套接字集合 fdSocket, 添加监听套接字句柄到这个集合
	fd_set fdSocket;
	FD_ZERO(&fdSocket);
	FD_SET(sListen, &fdSocket);

	char szText[] = " TCP Server Demo! \r\n";
	int SocketIndex = 0;
	while (TRUE)
	{
		// 2) 将fdSockete结合的一个拷贝fdRead传递给select函数，
		// 当有事件发生，select函数一处fdRead集合中没有未决I/O操作的套接字句柄，然后返回
		fd_set fdRead = fdSocket;
		int nRet = ::select(0, &fdRead, NULL, NULL, NULL);
		if (nRet > 0)
		{
			// 3) 通过将原来的fdSocket集合与select处理过的fdRead集合比较
			// 确定哪些套接字有未决I/O,并进一步出来这些i/o

			for (int i = 0; i < fdSocket.fd_count; i++)
			{
				if (FD_ISSET(fdSocket.fd_array[i], &fdRead))
				{
					if (fdSocket.fd_array[i] == sListen)     // （1）监听套节字接收到新连接
					{
						if (fdSocket.fd_count < FD_SETSIZE)
						{
							sockaddr_in addrRemote;
							int nAddrLen = sizeof(addrRemote);
							SOCKET sNew = ::accept(sListen, (SOCKADDR*)&addrRemote, &nAddrLen);
							FD_SET(sNew, &fdSocket);
							printf("接收到链接 (%s)  port:(%d)\n", ::inet_ntoa(addrRemote.sin_addr), addrRemote.sin_port);


							sockaddr_in guest;
							int guest_len = sizeof(guest);
							::getsockname(sNew, (struct sockaddr *)&guest, &guest_len);
							printf("getsockname 本地 (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);

							//sockaddr_in guest;
							 guest_len = sizeof(guest);
							::getpeername(sNew, (struct sockaddr *)&guest, &guest_len);
							printf("getpeername 外地 (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);

							sprintf_s(szText, "编号：%d \r\n", SocketIndex++);
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
						if (nRecv > 0) // （2）可读
						{
							szText[nRecv] = '\0';
							printf("接收到数据：%s \n", szText);
						}
						else
						{
							sockaddr_in guest;
							int guest_len = sizeof(guest);
							::getpeername(fdSocket.fd_array[i], (struct sockaddr *)&guest, &guest_len);
							printf("关闭socket (%s)  port:(%d)\n", ::inet_ntoa(guest.sin_addr), guest.sin_port);

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