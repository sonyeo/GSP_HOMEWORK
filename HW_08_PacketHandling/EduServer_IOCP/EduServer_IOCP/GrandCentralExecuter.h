#pragma once
#include "Exception.h"
#include "MPL.h"
#include "XTL.h"
#include "ThreadLocal.h"

class GrandCentralExecuter
{
public:
	typedef std::function<void()> GCETask;

	GrandCentralExecuter(): mRemainTaskCount(0)
	{}

	// void리턴 함수를 Task라고 명명
	void DoDispatch(const GCETask& task)
	{
		CRASH_ASSERT(LThreadType == THREAD_IO_WORKER); ///< 일단 IO thread 전용

		
		// 1개 이상의 작업이 있다는건, 처음 작업을 넣은 애가 다 처리중이므로,
		if (InterlockedIncrement64(&mRemainTaskCount) > 1)
		{
			/// 이미 누군가 작업중이면 큐에 넣어놓고 탈출
			mCentralTaskQueue.push(task);
		}
		else
		{
			/// 처음 진입한 놈이 책임지고 다해주자 -.-;

			mCentralTaskQueue.push(task);
			
			while (true)
			{
				GCETask task;
				if (mCentralTaskQueue.try_pop(task))
				{
					// Task를 수행!
					task();

					if (0 == InterlockedDecrement64(&mRemainTaskCount))
						break;
				}
			}
		}

	}


private:
	////TODO: concurrent_queue를 안써도 될텐데... 아닌가? 고민해보자
	typedef concurrency::concurrent_queue<GCETask, STLAllocator<GCETask>> CentralTaskQueue;
	CentralTaskQueue mCentralTaskQueue;
	int64_t mRemainTaskCount;
};

extern GrandCentralExecuter* GGrandCentralExecuter;


template <class T, class F, class... Args>
void GCEDispatch(T instance, F memfunc, Args&&... args)
{
	/// shared_ptr이 아닌 녀석은 받으면 안된다. 작업큐에 들어있는중에 없어질 수 있으니..
	static_assert(true == is_shared_ptr<T>::value, "T should be shared_ptr");

	// 멤버함수를 호출하려면 instance가 필요하고, 인자들도 필요한데, 다 bind해서 보관
	auto bind = std::bind(memfunc, instance, std::forward<Args>(args)...);
	GGrandCentralExecuter->DoDispatch(bind);
}