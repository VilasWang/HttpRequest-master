#include "ThreadPoolThread.h"
#include <process.h>
#include <cassert>
#include <iostream>
#include "ClassMemoryTracer.h"
#include "Log.h"
#include "ThreadPool.h"
#include "Task.h"

using namespace VCUtil;

ThreadPoolThread::ThreadPoolThread(ThreadPool* threadpool)
    : m_pThreadPool(threadpool)
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
    terminateTask();
}

UINT WINAPI ThreadPoolThread::threadFunc(LPVOID pParam)
{
    ThreadPoolThread* t = reinterpret_cast<ThreadPoolThread*>(pParam);
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

bool ThreadPoolThread::assignTask(std::unique_ptr<TaskBase> task)
{
    if (!task.get())
    {
        return false;
    }
    m_pTask = std::move(task);
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

bool ThreadPoolThread::runTask()
{
    resume();
    return true;
}

bool ThreadPoolThread::terminateTask()
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

        if (!isExit() && m_pThreadPool)
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

bool ActiveThreadList::append(std::unique_ptr<ThreadPoolThread> th)
{
    if (!th.get())
        return false;

    {
        Locker<CSLock> locker(m_lock);
        m_threads.emplace_back(std::move(th));
    }
    return true;
}

bool ActiveThreadList::remove(std::unique_ptr<ThreadPoolThread> th)
{
    if (!th.get())
        return false;

    {
        Locker<CSLock> locker(m_lock);
        m_threads.remove(th);
    }
    return true;
}

ThreadPoolThread* ActiveThreadList::get(int task_id)
{
    ThreadPoolThread* th = nullptr;
    {
        Locker<CSLock> locker(m_lock);
        auto iter = m_threads.begin();
        for (; iter != m_threads.end();)
        {
            if ((*iter)->taskId() == task_id)
            {
                th = iter->get();
                break;
            }
            else
            {
                ++iter;
            }
        }
    }
    return th;
}

std::unique_ptr<ThreadPoolThread> ActiveThreadList::take(int task_id)
{
    std::unique_ptr<ThreadPoolThread> th;
    {
        Locker<CSLock> locker(m_lock);
        auto iter = m_threads.begin();
        for (; iter != m_threads.end();)
        {
            if ((*iter)->taskId() == task_id)
            {
                th = std::move(*iter);
                iter = m_threads.erase(iter);
                break;
            }
            else
            {
                ++iter;
            }
        }
    }
    return th;
}

std::unique_ptr<ThreadPoolThread> ActiveThreadList::take(UINT thread_id)
{
    std::unique_ptr<ThreadPoolThread> th;
    {
        Locker<CSLock> locker(m_lock);
        auto iter = m_threads.begin();
        for (; iter != m_threads.end();)
        {
            if ((*iter)->threadId() == thread_id)
            {
                th = std::move(*iter);
                iter = m_threads.erase(iter);
                break;
            }
            else
            {
                ++iter;
            }
        }
    }
    return th;
}

std::unique_ptr<ThreadPoolThread> ActiveThreadList::pop_back()
{
    std::unique_ptr<ThreadPoolThread> th;
    {
        Locker<CSLock> locker(m_lock);
        if (!m_threads.empty())
        {
            th = std::move(m_threads.back());
            m_threads.remove(th);
        }
    }
    return th;
}

int ActiveThreadList::size()
{
    Locker<CSLock> locker(m_lock);
    return m_threads.size();
}

bool ActiveThreadList::isEmpty()
{
    Locker<CSLock> locker(m_lock);
    return m_threads.empty();
}

bool ActiveThreadList::clear()
{
    Locker<CSLock> locker(m_lock);
    m_threads.clear();
    return true;
}

void ActiveThreadList::stopAll()
{
    Locker<CSLock> locker(m_lock);
    auto iter = m_threads.begin();
    for (; iter != m_threads.end(); iter++)
    {
        if (nullptr != iter->get())
        {
            (*iter)->terminateTask();
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

std::unique_ptr<ThreadPoolThread> IdleThreadStack::pop()
{
    Locker<CSLock> locker(m_lock);
    if (!m_threads.empty())
    {
        std::unique_ptr<ThreadPoolThread> th = std::move(m_threads.top());
        m_threads.pop();
        return th;
    }
    return nullptr;
}

bool IdleThreadStack::push(std::unique_ptr<ThreadPoolThread> th)
{
    if (!th.get())
        return false;

    Locker<CSLock> locker(m_lock);
    th->suspend();
    m_threads.push(std::move(th));

    return true;
}

int IdleThreadStack::size()
{
    Locker<CSLock> locker(m_lock);
    return m_threads.size();
}

bool IdleThreadStack::isEmpty()
{
    Locker<CSLock> locker(m_lock);
    return m_threads.empty();
}

bool IdleThreadStack::clear()
{
    Locker<CSLock> locker(m_lock);
    while (!m_threads.empty())
    {
        m_threads.pop();
    }
    return true;
}