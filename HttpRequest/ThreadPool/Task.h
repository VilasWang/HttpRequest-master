#pragma once

#include <deque>
#include "Mutex.h"

class TaskBase
{
public:
    TaskBase(bool bAutoDelete = true);
    virtual ~TaskBase();

    virtual void exec() = 0;
    virtual void cancel() = 0;

    const int id();
	bool isAutoDelete();

protected:
    int m_id;

private:
    static int s_id;
	bool m_bAutoDelete;
};

class TaskQueue
{
public:
    TaskQueue();
    ~TaskQueue();

public:
    TaskBase* pop();
    bool push(TaskBase* t);
    bool pushFront(TaskBase* t);//²åµ½¶ÓÊ×¡£
    bool isEmpty();
    bool clear();

private:
    std::deque<TaskBase*>m_TaskQueue;
    TPLock m_lock;
};