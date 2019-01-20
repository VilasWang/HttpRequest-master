#include "HttpTask.h"
#include <iostream>
#ifdef TRACE_CLASS_MEMORY_ENABLED
#include "ClassMemoryTracer.h"
#endif


HttpTask::HttpTask(bool bAutoDelete) : TaskBase(bAutoDelete)
{
#ifdef TRACE_CLASS_MEMORY_ENABLED
	TRACE_CLASS_CONSTRUCTOR(HttpTask);
#endif
}

HttpTask::~HttpTask()
{
	std::cout << __FUNCTION__ << " id:" << m_id << std::endl;

#ifdef TRACE_CLASS_MEMORY_ENABLED
	TRACE_CLASS_DESTRUCTOR(HttpTask);
#endif

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