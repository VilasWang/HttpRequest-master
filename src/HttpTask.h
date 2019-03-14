#pragma once

#include "ThreadPool/Task.h"
#include "HttpRequest.h"
#include "httprequestdef.h"

// class HttpTask - Http«Î«Û»ŒŒÒ
class HttpTask : public TaskBase
{
public:
	HttpTask(bool bAutoDelete = true);
	~HttpTask();

	void attach(std::shared_ptr<CURLInterface>);
	void detach();
	void exec() override;
	void cancel() override;

private:
	std::shared_ptr<CURLInterface> m_request;
};