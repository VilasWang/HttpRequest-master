#pragma once

#include <functional>
#include "ThreadPoolThread.h"
#include "ScheduleThread.h"
#include "Task.h"

#define WM_THREAD_TASK_FINISHED (WM_USER + 1000)

// class ThreadPool - �̳߳�
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
    //��ʼ���̳߳أ�����n���̵߳��̳߳ء�
    bool init(int threadCount = 4);
    //ֹͣ���е����񣬲��ҽ������߳��˳���
    bool waitForDone();

    //priorityΪ���ȼ��������ȼ������񽫱����뵽����
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