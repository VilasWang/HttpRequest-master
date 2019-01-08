#include "HttpTask.h"

HttpTask::HttpTask(bool bAutoDelete) : TaskBase(bAutoDelete)
{
}

HttpTask::~HttpTask()
{
    /*char ch[64];
    sprintf_s(ch, "%s id:%d\n", __FUNCTION__, m_id);
    OutputDebugStringA(ch);*/

    detach();
}

void HttpTask::attach(const std::shared_ptr<RequestImpl>& request)
{
    m_lock.Lock();
    m_request = request;
    m_id = request->requestId();
    m_lock.UnLock();
}

void HttpTask::detach()
{
    m_lock.Lock();
    if (m_request != nullptr)
    {
        m_request.reset();
    }
    m_lock.UnLock();
}

void HttpTask::exec()
{
    m_lock.Lock();
    if (m_request != nullptr)
    {
        m_request->perform();
        m_request.reset();
    }
    m_lock.UnLock();
}

void HttpTask::cancel()
{
    if (m_request != nullptr)
    {
        m_request->cancel();
    }
}