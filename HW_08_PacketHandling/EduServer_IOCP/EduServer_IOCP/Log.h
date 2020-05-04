#pragma once

#include <iostream>
#include "Exception.h"
#include "ThreadLocal.h"

////TODO: mCounter 가 overflow 나지 않으려나?
class ThreadCallHistory
{
public:
	ThreadCallHistory(int tid) : mThreadId(tid)
	{
		memset(mHistory, 0, sizeof(mHistory));
	}

	////TODO: 이거 MAX까지 차면, 맨 아래꺼 다시 덮어쓸텐데, 이게 괜찮은가? 차선인가보다?
	// 이거 계속해서 무한히 남기는거임
	inline void Append(const char* funsig)
	{
		mHistory[mCounter++ % MAX_HISTORY] = funsig;
	}

	void DumpOut(std::ostream& ost = std::cout);

private:
	enum
	{
		MAX_HISTORY = 1024
	};

	uint64_t	mCounter = 0;
	int			mThreadId = -1;
	const char*	mHistory[MAX_HISTORY];
};

// main thread가 아니면, call history 보관하고,
// this 객체가 존재하면, LRecentThisPointer를 세팅해줌 -> 즉, 멤버함수안에서 하면, 해당 인스턴스가 세팅될듯
// Session, ClientSession의 함수마다 호출해줌
#define TRACE_THIS	\
	__if_exists (this)	\
	{	\
		LRecentThisPointer = (void*)this;	\
	}	\
	if (LThreadType != THREAD_MAIN)	\
	{	\
		LThreadCallHistory->Append(__FUNCSIG__);	\
	}	
	


class ThreadCallElapsedRecord
{
public:
	ThreadCallElapsedRecord(int tid) : mThreadId(tid)
	{
		memset(mElapsedFuncSig, 0, sizeof(mElapsedFuncSig));
		memset(mElapsedTime, 0, sizeof(mElapsedTime));
	}

	inline void Append(const char* funcsig, int64_t elapsed)
	{
		mElapsedFuncSig[mCounter % MAX_ELAPSED_RECORD] = funcsig;
		mElapsedTime[mCounter % MAX_ELAPSED_RECORD] = elapsed;
		++mCounter;
	}

	void DumpOut(std::ostream& ost = std::cout);

private:
	enum
	{
		MAX_ELAPSED_RECORD = 512
	};

	uint64_t	mCounter = 0;
	int			mThreadId = -1;
	const char*	mElapsedFuncSig[MAX_ELAPSED_RECORD];
	int64_t		mElapsedTime[MAX_ELAPSED_RECORD];
};

// LThreadCallElapsedRecord에 시간 정보를 자동으로 넣어주는!
////TODO: funcsig로 어떤 문자열을 넣어주나?
// __FUNCSIG__ 를 넣어주는군
class ScopeElapsedCheck
{
public:
	ScopeElapsedCheck(const char* funcsig) : mFuncSig(funcsig)
	{
		/* FYI
		 * 10~16 ms 해상도로 체크하려면 GetTickCount 사용
		 * 1 us 해상도로 체크하려면  QueryPerformanceCounter 사용
		*/ 
		mStartTick = GetTickCount64();
	}

	~ScopeElapsedCheck()
	{
		if (LThreadType != THREAD_MAIN)
			LThreadCallElapsedRecord->Append(mFuncSig, GetTickCount64() - mStartTick);
	}

private:

	const char*	mFuncSig;
	int64_t	mStartTick = 0;
};

// 이 매크로를 scope 안에 쓰면, 위에껄 쓰게 되는?
// https://docs.microsoft.com/ko-kr/cpp/preprocessor/predefined-macros?view=vs-2019
// __FUNCSIG__ : void __cdecl exampleFunction(void)
#define TRACE_PERF	\
	ScopeElapsedCheck __scope_elapsed_check__(__FUNCSIG__);


namespace LoggerUtil
{
	// 어떤 thread에서, msg(로그)를 남겼음! 추가정보는 int로
	struct LogEvent
	{
		int mThreadId = -1;
		int	mAdditionalInfo = 0;
		const char* mMessage = nullptr; 
	};

	#define MAX_LOG_SIZE  65536   ///< Must be a power of 2

	extern LogEvent gLogEvents[MAX_LOG_SIZE];
	extern __int64 gCurrentLogIndex;

	inline void EventLog(const char* msg, int info)
	{
		__int64 index = _InterlockedIncrement64(&gCurrentLogIndex) - 1;
		LogEvent& event = gLogEvents[index & (MAX_LOG_SIZE - 1)];
		event.mThreadId = LWorkerThreadId;
		event.mMessage = msg;
		event.mAdditionalInfo = info;
	}

	void EventLogDumpOut(std::ostream& ost = std::cout);
}

#define EVENT_LOG(x, info) LoggerUtil::EventLog(x, info)

