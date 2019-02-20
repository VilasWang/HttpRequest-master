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
	, m_hThread(INVALID_HANDLE_VALUE)
	, m_hEvent(nullptr)
	, m_nThreadID(0)
	, m_bExit(false)
	, m_bRunning(false)
#if _MSC_VER < 1700
	, m_lock(new CSLock)
#endif
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
#if _MSC_VER < 1700
	m_lock.reset();
#endif
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
			CSLocker locker(t->m_lock);
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
			CSLocker locker(t->m_lock);
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
	CSLocker locker(m_lock);
#endif
	return m_bRunning;
}

bool ThreadPoolThread::isExit() const
{
#if _MSC_VER < 1700
	CSLocker locker(m_lock);
#endif
	return m_bExit;
}

void ThreadPoolThread::setExit(bool bExit)
{
#if _MSC_VER < 1700
	CSLocker locker(m_lock);
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

	m_lock.lock();
	m_list.push_back(t);
	m_lock.unLock();
	return true;
}

bool ActiveThreadList::remove(ThreadPoolThread* t)
{
	if (!t)
	{
		return false;
	}

	m_lock.lock();
	m_list.remove(t);
	m_lock.unLock();
	return true;
}

ThreadPoolThread* ActiveThreadList::get(int task_id)
{
	ThreadPoolThread* t = nullptr;
	m_lock.lock();
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
	m_lock.unLock();
	return t;
}

ThreadPoolThread* ActiveThreadList::take(int task_id)
{
	ThreadPoolThread* t = nullptr;
	m_lock.lock();
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
	m_lock.unLock();
	return t;
}

ThreadPoolThread* ActiveThreadList::take(UINT thread_id)
{
	ThreadPoolThread* t = nullptr;
	m_lock.lock();
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
	m_lock.unLock();
	return t;
}

ThreadPoolThread* ActiveThreadList::pop_back()
{
	ThreadPoolThread* t = nullptr;
	m_lock.lock();
	if (!m_list.empty())
	{
		t = m_list.back();
		m_list.remove(t);
	}
	m_lock.unLock();
	return t;
}

int ActiveThreadList::size()
{
	m_lock.lock();
	int size = m_list.size();
	m_lock.unLock();
	return size;
}

bool ActiveThreadList::isEmpty()
{
	m_lock.lock();
	bool ret = m_list.empty();
	m_lock.unLock();
	return ret;
}

bool ActiveThreadList::clear()
{
	m_lock.lock();
	auto iter = m_list.begin();
	for (; iter != m_list.end(); iter++)
	{
		if (nullptr != (*iter))
		{
			delete (*iter);
		}
	}
	m_list.clear();
	m_lock.unLock();
	return true;
}

void ActiveThreadList::stopAll()
{
	m_lock.lock();
	auto iter = m_list.begin();
	for (; iter != m_list.end(); iter++)
	{
		if (nullptr != (*iter))
		{
			(*iter)->stopTask();
		}
	}
	m_lock.unLock();
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
	m_lock.lock();
	if (!m_stack.empty())
	{
		ThreadPoolThread* t = m_stack.top();
		m_stack.pop();
		m_lock.unLock();
		return t;
	}
	m_lock.unLock();
	return nullptr;
}

bool IdleThreadStack::push(ThreadPoolThread* t)
{
	assert(t);
	if (!t)
	{
		return false;
	}
	m_lock.lock();
	t->suspend();
	m_stack.push(t);
	m_lock.unLock();
	return true;
}

int IdleThreadStack::size()
{
	m_lock.lock();
	int size = m_stack.size();
	m_lock.unLock();
	return size;
}

bool IdleThreadStack::isEmpty()
{
	m_lock.lock();
	bool ret = m_stack.empty();
	m_lock.unLock();
	return ret;
}

bool IdleThreadStack::clear()
{
	m_lock.lock();
	ThreadPoolThread* pThread = nullptr;
	while (!m_stack.empty())
	{
		pThread = m_stack.top();
		m_stack.pop();

		if (pThread)
		{
			delete pThread;
		}
	}
	m_lock.unLock();
	return true;
}