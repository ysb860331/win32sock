#include <windows.h>

void main()
{
	HANDLE hEvent = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, "ShutdownEvent");
	if (hEvent != NULL)
	{
		::SetEvent(hEvent);
		::CloseHandle(hEvent);
		::MessageBox(NULL, " 服务器关闭成功！\n", "ServerShutdown", 0);
	}
	else
	{
		::MessageBox(NULL, " 服务器还没有启动！\n", "ServerShutdown", 0);
	}
}