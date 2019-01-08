#pragma once

#include "ThreadPoolThread.h"
#include "Task.h"

#define DEFAULT_THREAD_COUNT 4

class ThreadPool
{
public:
    enum Priority
    {
        Normal,
        High
    };

    ~ThreadPool();
    static ThreadPool* Instance();

public:
    //初始化线程池，创建n个线程的线程池。
    bool init(int threadCount = DEFAULT_THREAD_COUNT);
    //停止所有的任务，并且将所有线程退出。
    bool waitForDone();

    //priority为优先级。高优先级的任务将被插入到队首
    bool addTask(TaskBase* t, Priority p);

    bool abortTask(int taskId);
    bool abortAllTask();

    //线程任务完成通知取任务
    //将线程从活动队列取出，放入空闲线程栈中。
    //在取任务之前，先判断任务队列是否有任务,如为空时才挂起,否则从任务队列取任务继续执行
    virtual bool onThreadFinished(ThreadPoolThread*);

public:
    class ThreadPoolCallBack
    {
    public:
        virtual void onTaskFinished(int task_id) = 0;
    };

    void setCallBack(ThreadPoolCallBack* pCallBack);
    void onTaskFinished(int taskId);

protected:
    ThreadPool();
    virtual ThreadPoolThread* popIdleThread();
    virtual TaskBase* getNextTask();

private:
    int m_nThreadNum;
    bool m_bInitialized;
    ThreadPoolCallBack* m_pCallBack;

    IdleThreadStack m_idleThreads;
    ActiveThreadList m_activeThreads;
    TaskQueue m_taskQueue;
};