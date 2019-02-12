#include "HttpTask.h"
#include <iostream>
#include "ClassMemoryTracer.h"
#include "log.h"


HttpTask::HttpTask(bool bAutoDelete) : TaskBase(bAutoDelete)
{
	TRACE_CLASS_CONSTRUCTOR(HttpTask);
}

HttpTask::~HttpTask()
{
	//LOG_DEBUG("%s id[%d]\n", __FUNCTION__, m_id);
	TRACE_CLASS_DESTRUCTOR(HttpTask);

	detach();
}

void HttpTask::attach(std::shared_ptr<CURLInterface> request)
{
	m_request = request;
	m_id = request->requestId();
}

void HttpTask::detach()
{
	if (m_request.get())
	{
		m_request.reset();
	}
}

void HttpTask::exec()
{
	if (m_request.get())
	{
		m_request->perform();
		m_request.reset();
	}
}

void HttpTask::cancel()
{
	if (m_request.get())
	{
		m_request->cancel();
	}
}