#include "stdafx.h"
#include "Exception.h"
#include "ThreadLocal.h"
#include "Timer.h"
#include "EduServer_IOCP.h"
#include "IOThread.h"
#include "ClientSession.h"
#include "ServerSession.h"
#include "IocpManager.h"
#include "DBContext.h"

IOThread::IOThread(HANDLE hThread, HANDLE hCompletionPort) : mThreadHandle(hThread), mCompletionPort(hCompletionPort)
{
}


IOThread::~IOThread()
{
	CloseHandle(mThreadHandle);
}

DWORD IOThread::Run()
{

	while (true)
	{
		DoIocpJob();
		
		DoTimerJob();
	
		DoSendJob(); ///< aggregated sends

		//... ...
	}

	return 1;
}

void IOThread::DoIocpJob()
{
	DWORD dwTransferred = 0;
	LPOVERLAPPED overlapped = nullptr;
	
	ULONG_PTR completionKey = 0;

	int ret = GetQueuedCompletionStatus(mCompletionPort, &dwTransferred, (PULONG_PTR)&completionKey, &overlapped, GQCS_TIMEOUT);

	if (CK_DB_RESULT == completionKey)
	{
		/// DB 처리 결과가 담겨옴.. 이걸 처리
		DatabaseJobContext* dbContext = reinterpret_cast<DatabaseJobContext*>(overlapped);
		dbContext->OnResult();
		delete dbContext;
		return;
	}

	/// 아래로는 일반적인 I/O 처리

	OverlappedIOContext* context = reinterpret_cast<OverlappedIOContext*>(overlapped);
	
	Session* remote = context ? context->mSessionObject : nullptr;

	if (ret == 0 || dwTransferred == 0)
	{
		/// check time out first 
		if ( context == nullptr && GetLastError() == WAIT_TIMEOUT)
			return;

	
		if (context->mIoType == IO_RECV || context->mIoType == IO_SEND)
		{
			CRASH_ASSERT(nullptr != remote);

			/// In most cases in here: ERROR_NETNAME_DELETED(64)

			remote->DisconnectRequest(DR_COMPLETION_ERROR);

			DeleteIoContext(context);

			return;
		}
	}

	CRASH_ASSERT(nullptr != remote);

	bool completionOk = false;
	switch (context->mIoType)
	{
	case IO_CONNECT:
		dynamic_cast<ServerSession*>(remote)->ConnectCompletion();
		completionOk = true;
		break;

	case IO_DISCONNECT:
		remote->DisconnectCompletion(static_cast<OverlappedDisconnectContext*>(context)->mDisconnectReason);
		completionOk = true;
		break;

	case IO_ACCEPT:
		dynamic_cast<ClientSession*>(remote)->AcceptCompletion();
		completionOk = true;
		break;

	case IO_RECV_ZERO:
		// 처음엔 PreRecv(Recv 0)로 누군가 Send하면 그걸 감지할 수 있도록 하고(미리 큰 버퍼를 가지고 기다리면 page locking!)
		// Send한걸 이렇게 감지하면, 제대로 PostRecv()로 받음
		completionOk = remote->PostRecv();
		break;

	case IO_SEND:
		remote->SendCompletion(dwTransferred);

		if (context->mWsaBuf.len != dwTransferred)
			printf_s("Partial SendCompletion requested [%d], sent [%d]\n", context->mWsaBuf.len, dwTransferred);
		else
			completionOk = true;
		
		break;

	case IO_RECV:
		// 이 안에서, 패킷을 처리하는 로직이 동작
		remote->RecvCompletion(dwTransferred);
	
		/// for test
		//remote->EchoBack();
		
		completionOk = remote->PreRecv();

		break;

	default:
		printf_s("Unknown I/O Type: %d\n", context->mIoType);
		CRASH_ASSERT(false);
		break;
	}

	if (!completionOk)
	{
		/// connection closing
		remote->DisconnectRequest(DR_IO_REQUEST_ERROR);
	}

	DeleteIoContext(context);
	
}


void IOThread::DoSendJob()
{
	// 이거 IOCP 통해서 Send 요청한것들이 다 완료되어야 될텐데..
	////TODO: 무한루프를 여기서 돌아도 되는것인가?
	while (!LSendRequestSessionList->empty())
	{
		auto& session = LSendRequestSessionList->front();
	
		if (session->FlushSend())
		{
			/// true 리턴 되면 빼버린다.
			LSendRequestSessionList->pop_front();
		}
	}
	
}

void IOThread::DoTimerJob()
{
	// Timer thread를 따로 안두고, 여기서 처리하네?
	LTimer->DoTimerJob();
}