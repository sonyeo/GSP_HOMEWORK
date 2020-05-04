#pragma once

#include <iostream>
#include "Exception.h"
#include "ThreadLocal.h"

////TODO: mCounter �� overflow ���� ��������?
class ThreadCallHistory
{
public:
	ThreadCallHistory(int tid) : mThreadId(tid)
	{
		memset(mHistory, 0, sizeof(mHistory));
	}

	////TODO: �̰� MAX���� ����, �� �Ʒ��� �ٽ� ����ٵ�, �̰� ��������? �����ΰ�����?
	// �̰� ����ؼ� ������ ����°���
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

// main thread�� �ƴϸ�, call history �����ϰ�,
// this ��ü�� �����ϸ�, LRecentThisPointer�� �������� -> ��, ����Լ��ȿ��� �ϸ�, �ش� �ν��Ͻ��� ���õɵ�
// Session, ClientSession�� �Լ����� ȣ������
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

// LThreadCallElapsedRecord�� �ð� ������ �ڵ����� �־��ִ�!
////TODO: funcsig�� � ���ڿ��� �־��ֳ�?
// __FUNCSIG__ �� �־��ִ±�
class ScopeElapsedCheck
{
public:
	ScopeElapsedCheck(const char* funcsig) : mFuncSig(funcsig)
	{
		/* FYI
		 * 10~16 ms �ػ󵵷� üũ�Ϸ��� GetTickCount ���
		 * 1 us �ػ󵵷� üũ�Ϸ���  QueryPerformanceCounter ���
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

// �� ��ũ�θ� scope �ȿ� ����, ������ ���� �Ǵ�?
// https://docs.microsoft.com/ko-kr/cpp/preprocessor/predefined-macros?view=vs-2019
// __FUNCSIG__ : void __cdecl exampleFunction(void)
#define TRACE_PERF	\
	ScopeElapsedCheck __scope_elapsed_check__(__FUNCSIG__);


namespace LoggerUtil
{
	// � thread����, msg(�α�)�� ������! �߰������� int��
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

