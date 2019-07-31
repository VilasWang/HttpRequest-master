#include "Task.h"
#include "ClassMemoryTracer.h"

using namespace VCUtil;

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
    {
        Locker<CSLock> locker(m_lock);
        if (!m_queTasks.empty())
        {
            t = std::move(m_queTasks.front());
            m_queTasks.pop_front();
        }
    }
    return t;
}

bool TaskQueue::push(std::unique_ptr<TaskBase> t)
{
    if (!t.get())
        return false;

    Locker<CSLock> locker(m_lock);
    m_queTasks.emplace_back(std::move(t));
    return true;
}

bool TaskQueue::pushFront(std::unique_ptr<TaskBase> t)
{
    if (!t.get())
        return false;

    {
        Locker<CSLock> locker(m_lock);
        m_queTasks.emplace_front(std::move(t));
    }
    return true;
}

bool TaskQueue::isEmpty()
{
    Locker<CSLock> locker(m_lock);
    return m_queTasks.empty();
}

bool TaskQueue::clear()
{
    {
        Locker<CSLock> locker(m_lock);
        m_queTasks.clear();
    }
    return true;
}