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