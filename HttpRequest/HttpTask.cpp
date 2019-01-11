#include "HttpTask.h"
#include "ClassMemoryTracer.h"

HttpTask::HttpTask(bool bAutoDelete) : TaskBase(bAutoDelete)
{
	TRACE_CLASS_CONSTRUCTOR(HttpTask);
}

HttpTask::~HttpTask()
{
    /*char ch[64];
    sprintf_s(ch, "%s id:%d\n", __FUNCTION__, m_id);
    OutputDebugStringA(ch);*/
	TRACE_CLASS_DESTRUCTOR(HttpTask);

    detach();
}

void HttpTask::attach(const std::shared_ptr<RequestImpl>& request)
{
    m_lock.lock();
    m_request = request;
    m_id = request->requestId();
    m_lock.unLock();
}

void HttpTask::detach()
{
    m_lock.lock();
    if (m_request.get())
    {
        m_request.reset();
    }
    m_lock.unLock();
}

void HttpTask::exec()
{
    m_lock.lock();
    if (m_request.get())
    {
        m_request->perform();
        m_request.reset();
    }
    m_lock.unLock();
}

void HttpTask::cancel()
{
    if (m_request.get())
    {
        m_request->cancel();
    }
}