#ifndef __IOCP_H__
#define __IOCP_H__

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "../../CommLib/common/initsock.h"
#include <WinSock2.h>
#include <windows.h>
#include <MSWSock.h>

#define BUFFER_SIZE 1024
#define MAX_THREAD 2

struct CIOCPBuffer
{
	WSAOVERLAPPED ol;
	SOCKET sClient;
	char *buff;
	int nLen;
	ULONG nSequenceNumber;
	int nOperation;
#define OP_ACCEPT 1
#define OP_WRITE 2
#define OP_READ 3

	CIOCPBuffer *pNext;
};

struct CIOCPContext
{
	SOCKET s;
	SOCKADDR_IN addrLocal;
	SOCKADDR_IN addrRemote;
	BOOL bClosing;
	int nOutstandingRecv;
	int nOutstandingSend;

	ULONG nReadSequence;
	ULONG nCurrentReadSequence;
	CIOCPBuffer *pOutOfOrderReads;

	CRITICAL_SECTION Lock;
	CIOCPContext *pNext;
};

class CIOCPServer
{
public:
	CIOCPServer();
	~CIOCPServer();

	BOOL Start(int nPort = 4567, int nMaxConnections = 2000, int nMaxFreeBuffers = 200, int nMaxFreeContexts = 100, int nInitialReads = 4);
	void ShutDown();

	void CloseAConnection(CIOCPContext *pContext);
	void CloseAllConnections();

	ULONG GetCurrentConnection() { return m_nCurrentConnection; }
	BOOL SendText(CIOCPContext *pContext, char *pszText, int nLen);
protected:
	CIOCPBuffer *AllocateBuffer(int nLen);
	void ReleaseBuffer(CIOCPBuffer *pBuffer);
	CIOCPContext *AllocateContext(SOCKET s);
	void ReleaseContext(CIOCPContext *pContext);
	void FreeBuffers();
	void FreeContexts();
	BOOL AddAConnection(CIOCPContext *pContext);
	BOOL InsertPendingAccept(CIOCPBuffer *pBuffer);
	BOOL RemovePendingAccept(CIOCPBuffer *pBuffer);
	CIOCPBuffer *GetNextReadBuffer(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	BOOL PostAccept(CIOCPBuffer *pBuffer);
	BOOL PostSend(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	BOOL PostRecv(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	void HandleIO(DWORD dwKey, CIOCPBuffer *pBuffer, DWORD dwTrans, int nError);

	virtual void OnConnectionEstablished(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	virtual void OnConnectionClosing(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	virtual void OnConnectionError(CIOCPContext *pContext, CIOCPBuffer *pBuffer, int nError);
	virtual void OnReadCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	virtual void OnWriteCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);

protected:
	CIOCPBuffer *m_pFreeBufferList;
	CIOCPContext *m_pFreeContextList;
	int m_nFreeBufferCount;
	int m_nFreeContextCount;
	CRITICAL_SECTION m_FreeBufferListLock;
	CRITICAL_SECTION m_FreeContextListLock;

	CIOCPBuffer *m_pPendingAccepts;
	long m_nPendingAcceptCount;
	CRITICAL_SECTION m_PendingAcceptsLock;

	CIOCPContext *m_pConnectionList;
	int m_nCurrentConnection;
	CRITICAL_SECTION m_ConnectionListLock;

	HANDLE m_hAcceptEvent;
	HANDLE m_hRepostEvent;
	LONG m_nRepostCount;

	int m_nPort;

	int m_nInitialAccepts;
	int m_nInitialReads;
	int m_nMaxAccepts;
	int m_nMaxSends;
	int m_nMaxFreeBuffers;
	int m_nMaxFreeContexts;
	int m_nMaxConnections;

	HANDLE m_hListenThread;
	HANDLE m_hCompletion;
	SOCKET m_sListen;
	LPFN_ACCEPTEX m_lpfnAcceptEx;
	LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockaddrs;

	BOOL m_bShutDown;
	BOOL m_bServerStarted;

private:
	static DWORD WINAPI _ListenThreadProc(LPVOID lpParam);
	static DWORD WINAPI _WorkerThreadProc(LPVOID lpParam);
};

#endif