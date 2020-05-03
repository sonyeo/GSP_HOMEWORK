#include "stdafx.h"
#include "ThreadLocal.h"
#include "Exception.h"
#include "SyncExecutable.h"
#include "Timer.h"



Timer::Timer()
{
	// https://smok95.tistory.com/225
	// 구동된지 얼마나 지났나?
	LTickCount = GetTickCount64();
}


void Timer::PushTimerJob(SyncExecutablePtr owner, const TimerTask& task, uint32_t after)
{
	CRASH_ASSERT(LThreadType == THREAD_IO_WORKER);

	int64_t dueTimeTick = after + LTickCount;

	// 큐에, 작업요청한객체, task, 실행될 시간 넣어줌
	mTimerJobQueue.push(TimerJobElement(owner, task, dueTimeTick));
}


void Timer::DoTimerJob()
{
	/// thread tick update
	LTickCount = GetTickCount64(); // 구동된지 얼마나 지났나 계속 업데이트

	// 큐가 비어있지 않으면,
	while (!mTimerJobQueue.empty())
	{
		// 제일 위에꺼 빼서
		const TimerJobElement& timerJobElem = mTimerJobQueue.top();

		// 시간이 경과하지 않았으면, 작업 안하고 끗!
		// 이런식으로 하려면, 우선순위 큐여야 할 것 같은데? 맞네! priority_queue!
		if (LTickCount < timerJobElem.mExecutionTick)
			break;

		// 타이머가 다른 thread에서 도니까, 락을 걸고 해당 객체에 접근해야함!
		timerJobElem.mOwner->EnterLock();
		timerJobElem.mTask();
		timerJobElem.mOwner->LeaveLock();

		mTimerJobQueue.pop();
	}


}

