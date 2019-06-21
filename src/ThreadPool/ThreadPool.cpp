#include "ThreadPool.h"
#include <cassert>
#include <iostream>
#include "ClassMemoryTracer.h"
#include "log.h"

ThreadPool::ThreadPool()
    : m_nThreadNum(1)
    , m_bInitialized(false)
    , m_pCallBack(nullptr)
    , m_pThread(new ScheduleThread)
{
    TRACE_CLASS_CONSTRUCTOR(ThreadPool);
}

ThreadPool::~ThreadPool()
{
    LOG_DEBUG("%s (B)\n", __FUNCTION__);
    TRACE_CLASS_DESTRUCTOR(ThreadPool);

    waitForDone();
    if (m_pThread)
    {
        m_pThread->quit();
        m_pThread->wait();
        m_pThread.reset();
    }

    TRACE_CLASS_CHECK_LEAKS();
    LOG_DEBUG("%s (E)\n", __FUNCTION__);
}

ThreadPool* ThreadPool::globalInstance()
{
    static ThreadPool _instance;
    return &_instance;
}

bool ThreadPool::init(int threadCount)
{
    if (m_bInitialized)
        return false;

    if (threadCount < 1 || threadCount > 16)
    {
        LOG_DEBUG("%s failed! thread range(1-16).\n", __FUNCTION__);
        return false;
    }

    m_nThreadNum = threadCount;
    for (int i = 0; i < threadCount; i++)
    {
        std::unique_ptr<ThreadPoolThread> th(new ThreadPoolThread(this));
        th->start();
        m_idleThreads.push(std::move(th));
    }
    if (m_pThread.get())
    {
        m_pThread->start();
    }
    m_bInitialized = true;
    return true;
}

bool ThreadPool::waitForDone()
{
    m_bInitialized = false;
    m_pCallBack = nullptr;

    abortAllTask();
    m_idleThreads.clear();
    m_activeThreads.clear();

    return true;
}

bool ThreadPool::addTask(std::unique_ptr<TaskBase> t, Priority p)
{
    if (!t.get() || !m_bInitialized)
    {
        return false;
    }

    if (p == Normal)
    {
        m_taskQueue.push(std::move(t));	        //放入任务队列队尾
    }
    else if (p == High)
    {
        m_taskQueue.pushFront(std::move(t));	//高优先级任务放入任务队列队首
    }

    if (m_pThread.get() && m_pThread->isSuspend())
    {
        m_pThread->resume();
    }

    return true;
}

bool ThreadPool::abortTask(int taskId)
{
    ThreadPoolThread* th = m_activeThreads.get(taskId);
    if (th)
    {
        th->terminateTask();
        return true;
    }
    return false;
}

bool ThreadPool::abortAllTask()
{
    m_taskQueue.clear();
    m_activeThreads.stopAll();
    return true;
}

std::unique_ptr<TaskBase> ThreadPool::takeTask()
{
    if (m_taskQueue.isEmpty())
    {
        return nullptr;
    }
    return m_taskQueue.pop();
}

void ThreadPool::pushIdleThread(std::unique_ptr<ThreadPoolThread> t)
{
    m_idleThreads.push(std::move(t));
}

std::unique_ptr<ThreadPoolThread> ThreadPool::popIdleThread()
{
    return m_idleThreads.pop();
}

void ThreadPool::appendActiveThread(std::unique_ptr<ThreadPoolThread> t)
{
    m_activeThreads.append(std::move(t));
}

std::unique_ptr<ThreadPoolThread> ThreadPool::takeActiveThread(UINT threadId)
{
    return m_activeThreads.take(threadId);
}

void ThreadPool::setNotifyCallBack(std::function<void(int)> callback)
{
    if (callback)
    {
        m_pCallBack = callback;
    }
}

void ThreadPool::onTaskFinished(int taskId, UINT threadId)
{
    if (!m_bInitialized)
        return;

    if (m_pThread)
    {
        ::PostThreadMessage(m_pThread->threadId(), WM_THREAD_TASK_FINISHED, (WPARAM)threadId, 0);
    }

    if (m_pCallBack && taskId > 0)
    {
        m_pCallBack(taskId);
    }
}