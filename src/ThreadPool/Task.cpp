//#include "stdafx.h"
#include "Task.h"
#include "ClassMemoryTracer.h"

#if _MSC_VER >= 1700
std::atomic<int> TaskBase::s_id = 0;
#else
int TaskBase::s_id = 0;
#endif

TaskBase::TaskBase(bool bAutoDelete)
    : m_id(++s_id)
    , m_bAutoDelete(bAutoDelete)
{
    TRACE_CLASS_CONSTRUCTOR(TaskBase);
}

TaskBase::~TaskBase()
{
    TRACE_CLASS_DESTRUCTOR(TaskBase);
}

const int TaskBase::id() const
{
    return m_id;
}

bool TaskBase::isAutoDelete() const
{
    return m_bAutoDelete;
}

//////////////////////////////////////////////////////////////////////////
TaskQueue::TaskQueue(void)
{
}

TaskQueue::~TaskQueue(void)
{
    clear();
}

std::unique_ptr<TaskBase> TaskQueue::pop()
{
    std::unique_ptr<TaskBase> t = nullptr;
    Locker<CSLock> locker(m_lock);

    if (!m_TaskQueue.empty())
    {
        t = std::move(m_TaskQueue.front());
        m_TaskQueue.pop_front();
    }
    return t;
}

bool TaskQueue::push(std::unique_ptr<TaskBase> t)
{
    if (!t.get())
    {
        return false;
    }

    Locker<CSLock> locker(m_lock);
    m_TaskQueue.push_back(std::move(t));
    return true;
}

bool TaskQueue::pushFront(std::unique_ptr<TaskBase> t)
{
    if (!t.get())
    {
        return false;
    }

    Locker<CSLock> locker(m_lock);
    m_TaskQueue.push_front(std::move(t));
    return true;
}

bool TaskQueue::isEmpty()
{
    Locker<CSLock> locker(m_lock);
    return m_TaskQueue.empty();
}

bool TaskQueue::clear()
{
    Locker<CSLock> locker(m_lock);
    m_TaskQueue.clear();
    return true;
}