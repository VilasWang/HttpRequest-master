#ifndef THREADPOOLTHREAD_H
#define THREADPOOLTHREAD_H
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <list>
#include <stack>
#if _MSC_VER >= 1700
#include <atomic>
#endif
#include "Lock.h"


class TaskBase;
class ThreadPool;

//Class ThreadPoolThread - 线程池工作线程
class ThreadPoolThread
{
public:
    explicit ThreadPoolThread(ThreadPool* threadpool);
#if _MSC_VER >= 1700
    ThreadPoolThread(const ThreadPoolThread &) = delete;
    ThreadPoolThread &operator=(const ThreadPoolThread &) = delete;
#endif
    virtual ~ThreadPoolThread();

public:
    bool start();
    void quit();
    //线程挂起
    bool suspend();
    //线程挂起恢复
    bool resume();

    bool isRunning() const;

    const UINT threadId() const { return m_nThreadID; }
    const int taskId();

    //将任务关联到线程类
    bool assignTask(std::unique_ptr<TaskBase> pTask);
    bool runTask();
    bool terminateTask();

protected:
    virtual void exec();
    //尝试停止正在执行的任务，否则等待任务结束
    virtual void waitForDone();

private:
#if _MSC_VER < 1700
    ThreadPoolThread(const ThreadPoolThread &);
    ThreadPoolThread &operator=(const ThreadPoolThread &);
#endif
    static UINT WINAPI threadFunc(LPVOID pParam);

    bool isExit() const;
    void setExit(bool bExit);

private:
    HANDLE m_hThread;
    UINT m_nThreadID;
    HANDLE m_hEvent;
#if _MSC_VER >= 1700
    std::atomic<bool> m_bExit;
    std::atomic<bool> m_bRunning;
#else
    mutable CSLock m_lock;
    bool m_bExit;
    bool m_bRunning;
#endif

    std::unique_ptr<TaskBase> m_pTask;
    ThreadPool* m_pThreadPool;
};

class ActiveThreadList
{
public:
    ActiveThreadList();
    virtual ~ActiveThreadList();

public:
    bool append(std::unique_ptr<ThreadPoolThread> t);
    bool remove(std::unique_ptr<ThreadPoolThread> t);
    ThreadPoolThread* get(int task_id);
    std::unique_ptr<ThreadPoolThread> take(int task_id);
    std::unique_ptr<ThreadPoolThread> take(UINT thread_id);
    std::unique_ptr<ThreadPoolThread> pop_back();
    int size();
    bool isEmpty();
    bool clear();
    void stopAll();

private:
    std::list<std::unique_ptr<ThreadPoolThread>> m_threads;
    mutable VCUtil::CSLock m_lock;
};

class IdleThreadStack
{
public:
    IdleThreadStack();
    ~IdleThreadStack();

public:
    std::unique_ptr<ThreadPoolThread> pop();
    bool push(std::unique_ptr<ThreadPoolThread>);
    int size();
    bool isEmpty();
    bool clear();

private:
    std::stack<std::unique_ptr<ThreadPoolThread>> m_threads;
    mutable VCUtil::CSLock m_lock;
};

#endif