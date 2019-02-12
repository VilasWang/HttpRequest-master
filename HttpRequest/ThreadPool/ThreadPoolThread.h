#pragma once
#include <windows.h>
#include <list>
#include <stack>
#include <atomic>
#include "CSLock.h"

class TaskBase;
class ThreadPool;

//Class ThreadPoolThread - 线程池工作线程
class ThreadPoolThread
{
public:
	explicit ThreadPoolThread(ThreadPool* threadPool);
	ThreadPoolThread(const ThreadPoolThread &) = delete;
	ThreadPoolThread &operator=(const ThreadPoolThread &) = delete;
	~ThreadPoolThread();

public:
	bool start();
	void quit();
	//线程挂起
	bool suspend();
	//线程挂起恢复
	bool resume();

	const UINT threadId() const { return m_nThreadID; }
	const int taskId();

	//将任务关联到线程类
	bool assignTask(std::shared_ptr<TaskBase> pTask);
	bool startTask();
	bool stopTask();

protected:
	virtual void exec();
	//尝试停止正在执行的任务，否则等待任务结束
	virtual void waitForDone();

private:
	static UINT WINAPI threadFunc(LPVOID pParam);

private:
	HANDLE m_hThread;
	UINT m_nThreadID;
	HANDLE m_hEvent;
	std::atomic<bool> m_bExit;
	CSLock m_lock;

	std::shared_ptr<TaskBase> m_pTask;
	ThreadPool* m_pThreadPool;
};

class ActiveThreadList
{
public:
	ActiveThreadList();
	~ActiveThreadList();

public:
	bool append(ThreadPoolThread* t);
	bool remove(ThreadPoolThread* t);
	ThreadPoolThread* get(int task_id);
	ThreadPoolThread* take(int task_id);
	ThreadPoolThread* take(UINT thread_id);
	ThreadPoolThread* pop_back();
	int size();
	bool isEmpty();
	bool clear();
	void stopAll();

private:
	std::list<ThreadPoolThread*>m_list;
	CSLock m_lock;
};

class IdleThreadStack
{
public:
	IdleThreadStack();
	~IdleThreadStack();

public:
	ThreadPoolThread* pop();
	bool push(ThreadPoolThread*);
	int size();
	bool isEmpty();
	bool clear();

private:
	std::stack<ThreadPoolThread*> m_stack;
	CSLock m_lock;
};
