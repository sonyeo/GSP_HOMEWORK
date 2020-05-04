#pragma once

enum LockOrder
{
	LO_DONT_CARE = 0, // ±‚∫ª
	LO_FIRST_CLASS, // ClientSession
	LO_BUSINESS_CLASS, 
	LO_ECONOMLY_CLASS, // SyncExecutable
	LO_LUGGAGE_CLASS // Session¿« SendBufferLock
};

class LockOrderChecker;

class FastSpinlock
{
public:
	FastSpinlock(const int lockOrder=LO_DONT_CARE);
	~FastSpinlock();

	void EnterLock();
	void LeaveLock();
	
private:
	FastSpinlock(const FastSpinlock& rhs);
	FastSpinlock& operator=(const FastSpinlock& rhs);

	volatile long mLockFlag;

	const int mLockOrder;

	friend class LockOrderChecker;
};

class FastSpinlockGuard
{
public:
	FastSpinlockGuard(FastSpinlock& lock) : mLock(lock)
	{
		mLock.EnterLock();
	}

	~FastSpinlockGuard()
	{
		mLock.LeaveLock();
	}

private:
	FastSpinlock& mLock;
};

template <class TargetClass>
class ClassTypeLock
{
public:
	struct LockGuard
	{
		LockGuard()
		{
			TargetClass::mLock.EnterLock();
		}

		~LockGuard()
		{
			TargetClass::mLock.LeaveLock();
		}

	};

private:
	static FastSpinlock mLock;
	
	//friend struct LockGuard;
};

template <class TargetClass>
FastSpinlock ClassTypeLock<TargetClass>::mLock;