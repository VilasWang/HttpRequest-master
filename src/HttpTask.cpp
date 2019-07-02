#include "HttpTask.h"
#include <iostream>
#include "ClassMemoryTracer.h"
#include "Log.h"


HttpTask::HttpTask(bool bAutoDelete) : TaskBase(bAutoDelete)
{
    TRACE_CLASS_CONSTRUCTOR(HttpTask);
}

HttpTask::~HttpTask()
{
    ///LOG_DEBUG("%s id[%d]\n", __FUNCTION__, m_id);
    TRACE_CLASS_DESTRUCTOR(HttpTask);
    detach();
}

void HttpTask::attach(std::shared_ptr<HTTP::IRequest> request)
{
    m_interface = request;
    m_id = request->requestId();
}

void HttpTask::detach()
{
    if (m_interface.get())
    {
        m_interface.reset();
    }
}

void HttpTask::exec()
{
    if (m_interface.get())
    {
        m_interface->perform();
        m_interface.reset();
    }
}

void HttpTask::cancel()
{
    if (m_interface.get())
    {
        m_interface->cancel();
    }
}