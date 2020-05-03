#include "stdafx.h"
#include "ThreadLocal.h"
#include "Exception.h"
#include "DBContext.h"
#include "DBThread.h"
#include "IocpManager.h"

DBThread::DBThread(HANDLE hThread, HANDLE hCompletionPort)
: mDbThreadHandle(hThread), mDbCompletionPort(hCompletionPort)
{}

DBThread::~DBThread()
{
	CloseHandle(mDbThreadHandle);
}

DWORD DBThread::Run()
{
	while (true)
	{
		DoDatabaseJob();
	}

	return 1;
}

void DBThread::DoDatabaseJob()
{
	DWORD dwTransferred = 0;
	LPOVERLAPPED overlapped = nullptr;
	ULONG_PTR completionKey = 0;

	int ret = GetQueuedCompletionStatus(mDbCompletionPort, &dwTransferred, (PULONG_PTR)&completionKey, &overlapped, INFINITE);

	if (CK_DB_REQUEST != completionKey)
	{
		CRASH_ASSERT(false);
		return;
	}

	// context로 넘어온 정보를 가지고 실행 후, 결과를 다시 Post하는데, 이때에는 IO쪽에 Post 하겠지
	DatabaseJobContext* dbContext = reinterpret_cast<DatabaseJobContext*>(overlapped);
	dbContext->mSuccess = dbContext->SQLExecute();
	GIocpManager->PostDatabaseResult(dbContext);

}

