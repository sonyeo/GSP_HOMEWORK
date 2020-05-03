#pragma once

#include "FastSpinlock.h"
#include "Timer.h"


class SyncExecutable : public std::enable_shared_from_this<SyncExecutable>
{
public:
	SyncExecutable() : mLock(LO_ECONOMLY_CLASS)
	{}
	virtual ~SyncExecutable() {}

	// 바로 호출안하고, 이렇게 넘기는 이유가..
	// 여튼
	// R을 리턴하는 T클래스의 멤버함수에 Args를 인자로 넣어서 호출
	template <class R, class T, class... Args>
	R DoSync(R (T::*memfunc)(Args...), Args... args)
	{
		FastSpinlockGuard lockGuard(mLock);
		return (static_cast<T*>(this)->*memfunc)(args...);
	}

	// 특정 시간 뒤에 함수를 호출하도록(함수는 bind해서 보관)
	template <class T, class... Args>
	void DoSyncAfter(uint32_t after, void (T::*memfunc)(Args...), Args&&... args)
	{
		auto bind = std::bind(memfunc, static_cast<T*>(this), std::forward<Args>(args)...);
		LTimer->PushTimerJob(GetThisPtr(), bind, after);
	}

	void EnterLock() { mLock.EnterLock(); }
	void LeaveLock() { mLock.LeaveLock(); }

	SyncExecutablePtr GetThisPtr() { return shared_from_this(); }
private:

	FastSpinlock mLock;
};

