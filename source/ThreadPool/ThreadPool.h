#ifndef THREADPOOL_H
#define THREADPOOL_H
#pragma once

#include <functional>
#include "ThreadPoolThread.h"
#include "ScheduleThread.h"
#include "Task.h"

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

    virtual ~ThreadPool();
#if _MSC_VER >= 1700
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
#endif

    static ThreadPool* globalInstance();

public:
    //初始化线程池，创建n个线程的线程池。
    bool init(int threadCount = 4);
    //停止所有的任务，并且将所有线程退出。
    bool waitForDone();

    //priority为优先级。高优先级的任务将被插入到队首
    bool addTask(std::unique_ptr<TaskBase> t, Priority p = Normal);
    bool abortTask(int taskId);
    bool abortAllTask();

    bool hasTask() { return !m_taskQueue.isEmpty(); }
    bool hasIdleThread() { return !m_idleThreads.isEmpty(); }

public:
    void setNotifyCallBack(std::function<void(int)> callback);

protected:
    void onTaskFinished(int taskId, UINT threadId);

private:
    ThreadPool();
#if _MSC_VER < 1700
    ThreadPool(const ThreadPool &);
    ThreadPool &operator=(const ThreadPool &);
#endif

    std::unique_ptr<TaskBase> takeTask();
    std::unique_ptr<ThreadPoolThread> popIdleThread();
    std::unique_ptr<ThreadPoolThread> takeActiveThread(UINT threadId);
    void appendActiveThread(std::unique_ptr<ThreadPoolThread>);
    void pushIdleThread(std::unique_ptr<ThreadPoolThread>);

    friend class ScheduleThread;
    friend class ThreadPoolThread;

private:
    int m_nThreadNum;
#if _MSC_VER >= 1700
    std::atomic<bool> m_bInitialized;
    std::unique_ptr<ScheduleThread> m_pThread;
#else
    bool m_bInitialized;
    std::shared_ptr<ScheduleThread> m_pThread;
#endif
    IdleThreadStack m_idleThreads;
    ActiveThreadList m_activeThreads;
    TaskQueue m_taskQueue;
    std::function<void(int)> m_pCallBack;
};

#endif