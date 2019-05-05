#pragma once

#include <deque>
#include <memory>
#if _MSC_VER >= 1700
#include <atomic>
#endif
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
#if _MSC_VER >= 1700
    static std::atomic<int> s_id;
#else
	static int s_id;
#endif
	bool m_bAutoDelete;
};

class TaskQueue
{
public:
    TaskQueue();
    ~TaskQueue();

public:
	std::unique_ptr<TaskBase> pop();
    bool push(std::unique_ptr<TaskBase> t);
    bool pushFront(std::unique_ptr<TaskBase> t);//²åµ½¶ÓÊ×
    bool isEmpty();
    bool clear();

private:
    std::deque<std::unique_ptr<TaskBase>> m_TaskQueue;
    mutable CSLock m_lock;
};