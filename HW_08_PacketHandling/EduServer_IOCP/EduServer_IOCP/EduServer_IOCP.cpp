// EduServer_IOCP.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Exception.h"
#include "MemoryPool.h"
#include "ThreadLocal.h"
#include "LockOrderChecker.h"
#include "EduServer_IOCP.h"
#include "ServerSession.h"
#include "ClientSession.h"
#include "ClientSessionManager.h"
#include "IocpManager.h"
#include "DBManager.h"
#include "GrandCentralExecuter.h"


int _tmain(int argc, _TCHAR* argv[])
{
	LThreadType = THREAD_MAIN;

	/// for dump on crash
	SetUnhandledExceptionFilter(ExceptionFilter);

	/// Global Managers
	GMemoryPool = new MemoryPool;
	GClientSessionManager = new ClientSessionManager;
	GIocpManager = new IocpManager;
	GGrandCentralExecuter = new GrandCentralExecuter; // 이걸 하나만 두는건가? Actor별로 1개를 두어야..처음에 Task 넣은 thread가 순서 보장하면서 다 처리하도록 -> 그러면 concurrnet queue일 이유도 없어지나? 그건 아닌데.. 어쩌지? 여러 thread가 Task를 넣을 순 있으니.. 
	// 넣을 때, index부터 증가시키고 넣으면?
	GDatabaseManager = new DBManager;
	LLockOrderChecker = new LockOrderChecker(-1); ///< main thread도 락순서 체크
	

	if (false == GIocpManager->Initialize())
		return -1;

	if (false == GDatabaseManager->Initialize())
		return -1;

	if (false == GDatabaseManager->StartDatabaseThreads())
		return -1;

	if (false == GIocpManager->StartIoThreads())
		return -1;

	printf_s("Start Server\n");

 	ServerSession* testServerSession = xnew<ServerSession>(CONNECT_SERVER_ADDR, CONNECT_SERVER_PORT);
 	if (false == testServerSession->ConnectRequest())
 	{
 		printf_s("Connect Server [%s] Error \n", CONNECT_SERVER_ADDR);
 	}
	

	GIocpManager->StartAccept(); ///< block here...


	GDatabaseManager->Finalize();
	GIocpManager->Finalize();

	printf_s("End Server\n");

	xdelete(testServerSession);

	delete GDatabaseManager;
	delete GIocpManager;
	delete GClientSessionManager;

	return 0;
}

