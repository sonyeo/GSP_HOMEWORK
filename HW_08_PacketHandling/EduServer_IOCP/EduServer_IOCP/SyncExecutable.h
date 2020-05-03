#pragma once

#include "FastSpinlock.h"
#include "Timer.h"


class SyncExecutable : public std::enable_shared_from_this<SyncExecutable>
{
public:
	SyncExecutable() : mLock(LO_ECONOMLY_CLASS)
	{}
	virtual ~SyncExecutable() {}

	// �ٷ� ȣ����ϰ�, �̷��� �ѱ�� ������..
	// ��ư
	// R�� �����ϴ� TŬ������ ����Լ��� Args�� ���ڷ� �־ ȣ��
	template <class R, class T, class... Args>
	R DoSync(R (T::*memfunc)(Args...), Args... args)
	{
		FastSpinlockGuard lockGuard(mLock);
		return (static_cast<T*>(this)->*memfunc)(args...);
	}

	// Ư�� �ð� �ڿ� �Լ��� ȣ���ϵ���(�Լ��� bind�ؼ� ����)
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

