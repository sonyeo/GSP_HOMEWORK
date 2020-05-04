#include "stdafx.h"
#include "ThreadLocal.h"
#include "FastSpinlock.h"
#include "ClientSession.h"
#include "DBContext.h"


DatabaseJobContext::DatabaseJobContext(ClientSession* owner) : mSessionObject(owner), mSuccess(false)
{
	////TODO: 이건 왜 해줘야 하는거지?
	// Async한 작업을 하는데, 정작 당사자가 사라져버리면 처리가..
	mSessionObject->AddRef();
}

DatabaseJobContext::~DatabaseJobContext()
{
	// 생성자에서 Add/소멸자에서 Release해주는 패턴
	mSessionObject->ReleaseRef();
}

bool DatabaseJobContext::SQLExecute()
{
	CRASH_ASSERT(LThreadType == THREAD_DB_WORKER);

	return OnSQLExecute();
}

void DatabaseJobContext::OnResult()
{
	CRASH_ASSERT(LThreadType == THREAD_IO_WORKER);

	if (mSuccess)
		OnSuccess();
	else
		OnFail();
}



