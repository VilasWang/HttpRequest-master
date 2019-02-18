#pragma once

#include <deque>
#include <memory>
#include <atomic>
#include "Lock.h"

class TaskBase
{
public:
    explicit TaskBase(bool bAutoDelete = true);
    virtual ~TaskBase();

    virtual void exec() = 0;
    virtual void cancel() = 0;

    const int id() const;
	bool isAutoDelete() const;

protected:
	int m_id;

private:
    static std::atomic<int> s_id;
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
    bool pushFront(std::shared_ptr<TaskBase> t);//²åµ½¶ÓÊ×
    bool isEmpty();
    bool clear();

private:
    std::deque<std::shared_ptr<TaskBase>>m_TaskQueue;
    CSLock m_lock;
};