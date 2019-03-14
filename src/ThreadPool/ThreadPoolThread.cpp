//#include "stdafx.h"
#include "ThreadPoolThread.h"
#include "ThreadPool.h"
#include "Task.h"
#include <process.h>
#include <cassert>
#include <iostream>
#include "ClassMemoryTracer.h"
#include "log.h"

ThreadPoolThread::ThreadPoolThread(ThreadPool* threadPool)
	: m_pThreadPool(threadPool)
	, m_pTask(nullptr)
	, m_hEvent(nullptr)
	, m_hThread(INVALID_HANDLE_VALUE)
	, m_nThreadID(0)
	, m_bExit(false)
	, m_bRunning(false)
{
	TRACE_CLASS_CONSTRUCTOR(ThreadPoolThread);

	m_hEvent = CreateEvent(nullptr, false, false, nullptr);
	if (nullptr == m_hEvent)
	{
		LOG_DEBUG("ThreadPoolThread CreateEvent error! [%ul]\n", GetLastError());
	}
}

ThreadPoolThread::~ThreadPoolThread()
{
	TRACE_CLASS_DESTRUCTOR(ThreadPoolThread);
	LOG_DEBUG("%s id[%d]\n", __FUNCTION__, m_nThreadID);

	quit();
	if (m_hEvent)
	{
		CloseHandle(m_hEvent);
		m_hEvent = nullptr;
	}
}

bool ThreadPoolThread::start()
{
	m_hThread = (HANDLE)_beginthreadex(nullptr, 0, &ThreadPoolThread::threadFunc, this, 0, &m_nThreadID);
	if (m_hThread == INVALID_HANDLE_VALUE)
	{
		LOG_DEBUG("%s error! [%ul]\n", __FUNCTION__, GetLastError());
		return false;
	}
	setExit(false);
	return true;
}

void ThreadPoolThread::quit()
{
	setExit(true);
	waitForDone();

	if (m_hThread)
	{
		if (WaitForSingleObject(m_hThread, 5000) == WAIT_TIMEOUT)
		{
			LOG_DEBUG("ThreadPoolThread WaitForThread 5s TIMEOUT. id[%d]\n", m_nThreadID);
			_endthreadex(1);
		}

		CloseHandle(m_hThread);
		m_hThread = nullptr;
		m_nThreadID = 0;
	}
}

bool ThreadPoolThread::suspend()
{
	ResetEvent(m_hEvent);
	return true;
}

bool ThreadPoolThread::resume()
{
	SetEvent(m_hEvent);
	return true;
}

void ThreadPoolThread::waitForDone()
{
	stopTask();
}

UINT WINAPI ThreadPoolThread::threadFunc(LPVOID pParam)
{
	ThreadPoolThread* t = (ThreadPoolThread*)pParam;
	if (t)
	{
		{
#if _MSC_VER < 1700
			Locker<CSLock> locker(t->m_lock);
#endif
			t->m_bRunning = true;
		}

		while (!t->isExit())
		{
			DWORD ret = WaitForSingleObject(t->m_hEvent, INFINITE);
			switch (ret)
			{
			case WAIT_OBJECT_0:
				{
					t->exec();
				}
				break;
			case WAIT_FAILED:
				{
					LOG_DEBUG("ThreadPoolThread WaitForEvent error. [ul]\n", GetLastError());
				}
				break;
			default:
				break;
			}
		}

		{
#if _MSC_VER < 1700
			Locker<CSLock> locker(t->m_lock);
#endif
			t->m_bRunning = false;
		}
	}
	return 0;
}

bool ThreadPoolThread::assignTask(std::shared_ptr<TaskBase> pTask)
{
	if (!pTask)
	{
		return false;
	}
	m_pTask = pTask;
	return true;
}

const int ThreadPoolThread::taskId()
{
	if (m_pTask.get())
	{
		return m_pTask->id();
	}
	return 0;
}

bool ThreadPoolThread::startTask()
{
	resume();
	return true;
}

bool ThreadPoolThread::stopTask()
{
	if (m_pTask.get())
	{
		m_pTask->cancel();
	}
	resume();
	return true;
}

void ThreadPoolThread::exec()
{
	if (isExit())
		return;

	if (m_pTask.get())
	{
		int id = m_pTask->id();
		m_pTask->exec();
		m_pTask.reset();

		if (m_pThreadPool && !isExit())
		{
			m_pThreadPool->onTaskFinished(id, m_nThreadID);
		}
	}
}

bool ThreadPoolThread::isRunning() const
{
#if _MSC_VER < 1700
	Locker<CSLock> locker(m_lock);
#endif
	return m_bRunning;
}

bool ThreadPoolThread::isExit() const
{
#if _MSC_VER < 1700
	Locker<CSLock> locker(m_lock);
#endif
	return m_bExit;
}

void ThreadPoolThread::setExit(bool bExit)
{
#if _MSC_VER < 1700
	Locker<CSLock> locker(m_lock);
#endif
	m_bExit = bExit;
}

//////////////////////////////////////////////////////////////////////////
ActiveThreadList::ActiveThreadList()
{
}

ActiveThreadList::~ActiveThreadList()
{
	clear();
}

bool ActiveThreadList::append(ThreadPoolThread* t)
{
	if (!t)
	{
		return false;
	}

	Locker<CSLock> locker(m_lock);
	m_list.push_back(t);
	return true;
}

bool ActiveThreadList::remove(ThreadPoolThread* t)
{
	if (!t)
	{
		return false;
	}

	Locker<CSLock> locker(m_lock);
	m_list.remove(t);
	return true;
}

ThreadPoolThread* ActiveThreadList::get(int task_id)
{
	ThreadPoolThread* t = nullptr;
	Locker<CSLock> locker(m_lock);
	auto iter = m_list.begin();
	for (; iter != m_list.end();)
	{
		if ((*iter)->taskId() == task_id)
		{
			t = (*iter);
			break;
		}
		else
		{
			++iter;
		}
	}
	return t;
}

ThreadPoolThread* ActiveThreadList::take(int task_id)
{
	ThreadPoolThread* t = nullptr;
	Locker<CSLock> locker(m_lock);
	auto iter = m_list.begin();
	for (; iter != m_list.end();)
	{
		if ((*iter)->taskId() == task_id)
		{
			t = (*iter);
			iter = m_list.erase(iter);
			break;
		}
		else
		{
			++iter;
		}
	}
	return t;
}

ThreadPoolThread* ActiveThreadList::take(UINT thread_id)
{
	ThreadPoolThread* t = nullptr;
	Locker<CSLock> locker(m_lock);
	auto iter = m_list.begin();
	for (; iter != m_list.end();)
	{
		if ((*iter)->threadId() == thread_id)
		{
			t = (*iter);
			iter = m_list.erase(iter);
			break;
		}
		else
		{
			++iter;
		}
	}
	return t;
}

ThreadPoolThread* ActiveThreadList::pop_back()
{
	ThreadPoolThread* t = nullptr;
	Locker<CSLock> locker(m_lock);
	if (!m_list.empty())
	{
		t = m_list.back();
		m_list.remove(t);
	}
	return t;
}

int ActiveThreadList::size()
{
	Locker<CSLock> locker(m_lock);
	return m_list.size();
}

bool ActiveThreadList::isEmpty()
{
	Locker<CSLock> locker(m_lock);
	return m_list.empty();
}

bool ActiveThreadList::clear()
{
	Locker<CSLock> locker(m_lock);
	auto iter = m_list.begin();
	for (; iter != m_list.end(); iter++)
	{
		if (nullptr != (*iter))
		{
			delete (*iter);
		}
	}
	m_list.clear();
	return true;
}

void ActiveThreadList::stopAll()
{
	Locker<CSLock> locker(m_lock);
	auto iter = m_list.begin();
	for (; iter != m_list.end(); iter++)
	{
		if (nullptr != (*iter))
		{
			(*iter)->stopTask();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IdleThreadStack::IdleThreadStack()
{
}

IdleThreadStack::~IdleThreadStack()
{
	clear();
}

ThreadPoolThread* IdleThreadStack::pop()
{
	Locker<CSLock> locker(m_lock);
	if (!m_stack.empty())
	{
		ThreadPoolThread* t = m_stack.top();
		m_stack.pop();
		return t;
	}
	return nullptr;
}

bool IdleThreadStack::push(ThreadPoolThread* t)
{
	assert(t);
	if (!t)
	{
		return false;
	}

	Locker<CSLock> locker(m_lock);
	t->suspend();
	m_stack.push(t);

	return true;
}

int IdleThreadStack::size()
{
	Locker<CSLock> locker(m_lock);
	return m_stack.size();
}

bool IdleThreadStack::isEmpty()
{
	Locker<CSLock> locker(m_lock);
	return m_stack.empty();
}

bool IdleThreadStack::clear()
{
	ThreadPoolThread* pThread = nullptr;
	Locker<CSLock> locker(m_lock);

	while (!m_stack.empty())
	{
		pThread = m_stack.top();
		m_stack.pop();

		if (pThread)
		{
			delete pThread;
		}
	}
	return true;
}