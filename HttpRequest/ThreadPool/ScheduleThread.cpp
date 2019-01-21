//#include "stdafx.h"
#include "ScheduleThread.h"
#include <process.h>
#include <iostream>
#include "ThreadPool.h"
#ifdef TRACE_CLASS_MEMORY_ENABLED
#include "ClassMemoryTracer.h"
#endif


ScheduleThread::ScheduleThread()
	: m_hThread(nullptr)
	, m_nThreadID(0)
	, m_bExit(false)
	, m_bRunning(false)
{
#ifdef TRACE_CLASS_MEMORY_ENABLED
	TRACE_CLASS_CONSTRUCTOR(ThreadPoolThread);
#endif
	m_hEvent = CreateEvent(0, TRUE, TRUE, 0);
	if (nullptr == m_hEvent)
	{
		std::cout << GetLastError() << std::endl;
	}
}

ScheduleThread::~ScheduleThread()
{
#ifdef TRACE_CLASS_MEMORY_ENABLED
	TRACE_CLASS_DESTRUCTOR(ThreadPoolThread);
#endif
	std::cout << __FUNCTION__ << " id:" << m_nThreadID << std::endl;

	quit();
	if (m_hEvent)
	{
		if (WaitForSingleObject(m_hEvent, 1000) == WAIT_TIMEOUT)
		{
			std::cout << "ScheduleThread WaitForSingleObject Event 1s TIMEOUT." << std::endl;
		}

		CloseHandle(m_hEvent);
		m_hEvent = nullptr;
	}
}

bool ScheduleThread::start()
{
	m_bExit = false;
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, &ScheduleThread::ThreadFunc, this, NULL, &m_nThreadID);
	if (m_hThread == INVALID_HANDLE_VALUE)
	{
		std::cout << GetLastError() << std::endl;
		return false;
	}
	return true;
}

void ScheduleThread::quit()
{
	m_bExit = true;
	resume();

	if (m_hThread)
	{
		if (WaitForSingleObject(m_hThread, 5000) == WAIT_TIMEOUT)
		{
			std::cout << "ScheduleThread WaitForSingleObject Thread 5s TIMEOUT." << std::endl;
			_endthreadex(1);
		}

		CloseHandle(m_hThread);
		m_hThread = nullptr;
		m_nThreadID = 0;
	}
}

bool ScheduleThread::wait(unsigned long time)
{
	if (nullptr == m_hThread)
		return true;

	if (time == ULONG_MAX)
	{
		WaitForSingleObject(m_hThread, INFINITE);
		return true;
	}

	if (WaitForSingleObject(m_hThread, time) == WAIT_TIMEOUT)
	{
		std::cout << "ScheduleThread::wait TIMEOUT. id:" << m_nThreadID << std::endl;
		_endthreadex(1);
		return false;
	}
	return true;
}

bool ScheduleThread::suspend()
{
	if (m_hEvent)
	{
		ResetEvent(m_hEvent);
		return true;
	}
	return false;
}

bool ScheduleThread::resume()
{
	if (m_hEvent)
	{
		SetEvent(m_hEvent);
		return true;
	}
	return false;
}

bool ScheduleThread::isSuspend()
{
	if (nullptr == m_hThread || nullptr == m_hEvent)
		return false;

	if (WAIT_OBJECT_0 != WaitForSingleObject(m_hEvent, 0))
		return true;

	return false;
}

unsigned __stdcall ScheduleThread::ThreadFunc(LPVOID pParam)
{
	ScheduleThread *t = (ScheduleThread *)(pParam);
	if (t)
	{
		t->m_bRunning = true;
		t->onBeforeExec();

		MSG msg = { 0 };
		DWORD ret = WAIT_FAILED;
		BOOL hasMsg = FALSE;
		while (!t->m_bExit)
		{
			ret = WaitForSingleObject(t->m_hEvent, INFINITE);
			switch (ret)
			{
			case WAIT_OBJECT_0:
				{
					msg = { 0 };
					hasMsg = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
					if (hasMsg)
					{
						switch (msg.message)
						{
						case WM_THREAD_TASK_FINISHED:
							t->switchToIdleThread((UINT)msg.wParam);
							break;
						case WM_QUIT:
							t->m_bExit = true;
							break;
						default:
							break;
						}
					}
					t->run();
				}
				break;
			case WAIT_FAILED:
				{
					std::cout << GetLastError() << std::endl;
				}
				break;
			default:
				break;
			}
		}

		t->onBeforeExit();
		t->m_bRunning = false;
	}
	return 0;
}

void ScheduleThread::run()
{
	if (m_bExit)
		return;

	if (!ThreadPool::globalInstance()->hasTask())
	{
		suspend();
		return;
	}

	if (!ThreadPool::globalInstance()->hasIdleThread())
	{
		//有任务但是没有空闲线程。调用Sleep(0), 放弃当前cpu时间片，调度进程切换可调度线程
		Sleep(0);//SwitchToThread();
		return;
	}

	std::shared_ptr<TaskBase> pTask = ThreadPool::globalInstance()->takeTask();
	if (pTask.get())
	{
		ThreadPoolThread* t = ThreadPool::globalInstance()->popIdleThread();
		if (nullptr != t)
		{
			ThreadPool::globalInstance()->appendActiveThread(t);
			t->assignTask(pTask);
			t->resume();
			Sleep(1);
		}
		else
		{
			OutputDebugString(L"ScheduleThread error1\n");
		}
	}
	else
	{
		OutputDebugString(L"ScheduleThread error2\n");
	}
}

void ScheduleThread::switchToIdleThread(UINT threadId)
{
	ThreadPoolThread* t = ThreadPool::globalInstance()->takeActiveThread(threadId);
	if (nullptr != t)
	{
		ThreadPool::globalInstance()->pushIdleThread(t);
	}
	else
	{
		OutputDebugString(L"ScheduleThread error3\n");
	}
}