#pragma once

#include "ThreadPool/Task.h"
#include "ThreadPool/mutex.h"
#include "HttpRequest.h"

class HttpTask : public TaskBase
{
public:
	HttpTask(bool bAutoDelete = true);
	~HttpTask();

	void attach(const std::shared_ptr<RequestImpl>&);
	void detach();
	void exec() override;
	void cancel() override;

private:
	std::shared_ptr<RequestImpl> m_request;
	TPLock m_lock;
};