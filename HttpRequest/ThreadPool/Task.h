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
	std::shared_ptr<TaskBase> pop();
    bool push(std::shared_ptr<TaskBase> t);
    bool pushFront(std::shared_ptr<TaskBase> t);//²åµ½¶ÓÊ×¡£
    bool isEmpty();
    bool clear();

private:
    std::deque<std::shared_ptr<TaskBase>>m_TaskQueue;
    TPLock m_lock;
};