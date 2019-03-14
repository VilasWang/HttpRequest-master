#pragma once

#include "ThreadPoolThread.h"
#include "ScheduleThread.h"
#include "Task.h"

#define DEFAULT_THREAD_COUNT 4
#define WM_THREAD_TASK_FINISHED (WM_USER + 1000)

// class ThreadPool - 线程池
class ThreadPool
{
public:
	enum Priority
	{
		Normal,
		High
	};

	~ThreadPool();
#if _MSC_VER >= 1700
	ThreadPool(const ThreadPool &) = delete;
	ThreadPool &operator=(const ThreadPool &) = delete;
#endif

	static ThreadPool* globalInstance();

public:
	//初始化线程池，创建n个线程的线程池。
	bool init(int threadCount = DEFAULT_THREAD_COUNT);
	//停止所有的任务，并且将所有线程退出。
	bool waitForDone();

	//priority为优先级。高优先级的任务将被插入到队首
	bool addTask(std::shared_ptr<TaskBase> t, Priority p = Normal);
	bool abortTask(int taskId);
	bool abortAllTask();

	bool hasTask() { return !m_taskQueue.isEmpty(); }
	bool hasIdleThread() { return !m_idleThreads.isEmpty(); }

public:
	class ThreadPoolCallBack
	{
	public:
		virtual void onTaskFinished(int task_id) = 0;
	};

	void setCallBack(ThreadPoolCallBack* pCallBack);
	void onTaskFinished(int taskId, UINT threadId);

private:
	ThreadPool();
#if _MSC_VER < 1700
	ThreadPool(const ThreadPool &);
	ThreadPool &operator=(const ThreadPool &);
#endif

	std::shared_ptr<TaskBase> takeTask();
	ThreadPoolThread* popIdleThread();
	ThreadPoolThread* takeActiveThread(UINT threadId);
	void appendActiveThread(ThreadPoolThread*);
	void pushIdleThread(ThreadPoolThread*);

	friend class ScheduleThread;

private:
	int m_nThreadNum;
#if _MSC_VER >= 1700
	std::atomic<bool> m_bInitialized;
#else
	bool m_bInitialized;
#endif
	ThreadPoolCallBack* m_pCallBack;
	ScheduleThread *m_pThread;
	IdleThreadStack m_idleThreads;
	ActiveThreadList m_activeThreads;
	TaskQueue m_taskQueue;
};