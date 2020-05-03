#include "stdafx.h"
#include "ThreadLocal.h"
#include "Exception.h"
#include "SyncExecutable.h"
#include "Timer.h"



Timer::Timer()
{
	// https://smok95.tistory.com/225
	// �������� �󸶳� ������?
	LTickCount = GetTickCount64();
}


void Timer::PushTimerJob(SyncExecutablePtr owner, const TimerTask& task, uint32_t after)
{
	CRASH_ASSERT(LThreadType == THREAD_IO_WORKER);

	int64_t dueTimeTick = after + LTickCount;

	// ť��, �۾���û�Ѱ�ü, task, ����� �ð� �־���
	mTimerJobQueue.push(TimerJobElement(owner, task, dueTimeTick));
}


void Timer::DoTimerJob()
{
	/// thread tick update
	LTickCount = GetTickCount64(); // �������� �󸶳� ������ ��� ������Ʈ

	// ť�� ������� ������,
	while (!mTimerJobQueue.empty())
	{
		// ���� ������ ����
		const TimerJobElement& timerJobElem = mTimerJobQueue.top();

		// �ð��� ������� �ʾ�����, �۾� ���ϰ� ��!
		// �̷������� �Ϸ���, �켱���� ť���� �� �� ������? �³�! priority_queue!
		if (LTickCount < timerJobElem.mExecutionTick)
			break;

		// Ÿ�̸Ӱ� �ٸ� thread���� ���ϱ�, ���� �ɰ� �ش� ��ü�� �����ؾ���!
		timerJobElem.mOwner->EnterLock();
		timerJobElem.mTask();
		timerJobElem.mOwner->LeaveLock();

		mTimerJobQueue.pop();
	}


}

