#include "stdafx.h"
#include "Exception.h"
#include "ThreadLocal.h"
#include "Log.h"
#include "EduServer_IOCP.h"
#include "OverlappedIOContext.h"
#include "Session.h"
#include "IocpManager.h"

__declspec(thread) std::deque<Session*>* LSendRequestSessionList = nullptr;

Session::Session(size_t sendBufSize, size_t recvBufSize) 
: mSendBufferLock(LO_LUGGAGE_CLASS), mSendBuffer(sendBufSize), mRecvBuffer(recvBufSize), mConnected(0), mRefCount(0), mSendPendingCount(0)
{
	mSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
}


// 서버에서 강제로 연결 종료를 시작하는 함수
void Session::DisconnectRequest(DisconnectReason dr)
{
	TRACE_THIS;

	/// 이미 끊겼거나 끊기는 중이거나
	if (0 == InterlockedExchange(&mConnected, 0))
		return;

	OverlappedDisconnectContext* context = new OverlappedDisconnectContext(this, dr);

	if (FALSE == DisconnectEx(mSocket, (LPWSAOVERLAPPED)context, TF_REUSE_SOCKET, 0))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DeleteIoContext(context);
			printf_s("Session::DisconnectRequest Error : %d\n", GetLastError());
		}
	}

}


bool Session::PreRecv()
{
	TRACE_THIS;

	if (!IsConnected())
		return false;

	OverlappedPreRecvContext* recvContext = new OverlappedPreRecvContext(this);

	DWORD recvbytes = 0;
	DWORD flags = 0;
	recvContext->mWsaBuf.len = 0;
	recvContext->mWsaBuf.buf = nullptr;

	/// start async recv
	if (SOCKET_ERROR == WSARecv(mSocket, &recvContext->mWsaBuf, 1, &recvbytes, &flags, (LPWSAOVERLAPPED)recvContext, NULL))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DeleteIoContext(recvContext);
			printf_s("Session::PreRecv Error : %d\n", GetLastError());
			return false;
		}
	}

	return true;
}

bool Session::PostRecv()
{
	TRACE_THIS;

	if (!IsConnected())
		return false;

	if (0 == mRecvBuffer.GetFreeSpaceSize())
		return false;

	OverlappedRecvContext* recvContext = new OverlappedRecvContext(this);

	DWORD recvbytes = 0;
	DWORD flags = 0;
	recvContext->mWsaBuf.len = (ULONG)mRecvBuffer.GetFreeSpaceSize();
	recvContext->mWsaBuf.buf = mRecvBuffer.GetBuffer();


	/// start real recv
	if (SOCKET_ERROR == WSARecv(mSocket, &recvContext->mWsaBuf, 1, &recvbytes, &flags, (LPWSAOVERLAPPED)recvContext, NULL))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DeleteIoContext(recvContext);
			printf_s("Session::PostRecv Error : %d\n", GetLastError());
			return false;
		}

	}

	return true;
}


bool Session::PostSend(const char* data, size_t len)
{
	TRACE_THIS;

	if (!IsConnected())
		return false;

	FastSpinlockGuard criticalSection(mSendBufferLock);

	if (mSendBuffer.GetFreeSpaceSize() < len)
		return false;

	/// flush later...
	LSendRequestSessionList->push_back(this);
	
	char* destData = mSendBuffer.GetBuffer();

	memcpy(destData, data, len);

	mSendBuffer.Commit(len);

	return true;
}


bool Session::FlushSend()
{
	TRACE_THIS;

	if (!IsConnected())
	{
		DisconnectRequest(DR_SENDFLUSH_ERROR);
		return true;
	}
		

	FastSpinlockGuard criticalSection(mSendBufferLock);

	/// 보낼 데이터가 없는 경우
	if (0 == mSendBuffer.GetContiguiousBytes())
	{
		/// 보낼 데이터도 없는 경우
		if (0 == mSendPendingCount)
			return true; // 보낼 데이터도 버퍼에 없고, Pending중인것도 없다. 깔끔하게 flush 되었음! true
		
		return false; // 보낼 데이터가 없는데, pending중인게 있으므로, 아직 깔끔하지 않다! false
	}

	// 이미 WSASend()요청은 했고, 그에 해당하는 IOCP callback이 호출되지 않았으므로,
	// 기다린다. 아직 IOCP 완료안되고 pending된게 있으므로 false
	/// 이전의 send가 완료 안된 경우
	if (mSendPendingCount > 0)
		return false;

	
	OverlappedSendContext* sendContext = new OverlappedSendContext(this);

	DWORD sendbytes = 0;
	DWORD flags = 0;
	sendContext->mWsaBuf.len = (ULONG)mSendBuffer.GetContiguiousBytes();
	sendContext->mWsaBuf.buf = mSendBuffer.GetBufferStart();

	/// start async send
	// send 버퍼를 거치지않고 바로 쓰도록 sendbytes를 0으로 세팅하고, 모든걸 그냥 바로 보내버림
	if (SOCKET_ERROR == WSASend(mSocket, &sendContext->mWsaBuf, 1, &sendbytes, flags, (LPWSAOVERLAPPED)sendContext, NULL))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DeleteIoContext(sendContext);
			printf_s("Session::FlushSend Error : %d\n", GetLastError());

			DisconnectRequest(DR_SENDFLUSH_ERROR);
			return true;
		}

	}

	// WSASend()는 바로 보낸게 아니라서, IOCP에서 진짜 보내졌다고 할때까진 pending 인가?
	mSendPendingCount++;

	// 무조건 1일수밖에 없을듯.. 위에서 >0 인경우는 걸렀으니
	// 다른 Actor(Session)에게 Send()를 하는 경우로 인해 남이 나에게 Send거리를 넣어놓고,
	// 다른 thread에서도 Flush를 하고 있는 경우?
	// 다른 Actor가 Send까지 하면 안되고, 해당 Actor의 Executor에 넣어줘야..
	return mSendPendingCount == 1;
}

void Session::DisconnectCompletion(DisconnectReason dr)
{
	TRACE_THIS;

	OnDisconnect(dr);

	/// release refcount when added at issuing a session
	ReleaseRef();
}


void Session::SendCompletion(DWORD transferred)
{
	TRACE_THIS;

	FastSpinlockGuard criticalSection(mSendBufferLock);

	mSendBuffer.Remove(transferred);

	////TODO: 이게 어떤 용도?
	// 이건 Send를 한번에 flush하면서, WSASend()를 해주는데, 그렇게 해줬다고 해서 바로 Send된게 아님!
	// IOCP를 통해서 여기까지 와야, 진짜로 보내진거라 pending을 줄여줌
	mSendPendingCount--;
}

void Session::RecvCompletion(DWORD transferred)
{
	TRACE_THIS;

	mRecvBuffer.Commit(transferred);

	// 다 받았으면, protobuf 써서 읽는 함수 호출!
	OnReceive(transferred);
}


// Session을 참조하는 곳이 없으면, 자동으로 OnRelease가 불리도록
// Async한 요청들도 있는데, 정작 처리하려고 보니, 해당 객체가 없으면 안되니?
// Accept시, DBContext 만들시(DB요청시), OverlappedIoContext 만들시(IO요청시)
void Session::AddRef()
{
	CRASH_ASSERT(InterlockedIncrement(&mRefCount) > 0);
}

// Disconnect 되었다고 바로 Session을 지우진 않고, 비동기 IO들이 끝나야!
// Disconnect시, DBContext 제거시, OverlappedIoContext 제거시
void Session::ReleaseRef()
{
	long ret = InterlockedDecrement(&mRefCount);
	CRASH_ASSERT(ret >= 0);

	if (ret == 0)
	{
		OnRelease();
	}
}

// 이제 안씀
void Session::EchoBack()
{
	TRACE_THIS;

	size_t len = mRecvBuffer.GetContiguiousBytes();

	if (len == 0)
		return;

	if (false == PostSend(mRecvBuffer.GetBufferStart(), len))
		return;

	mRecvBuffer.Remove(len);

}

