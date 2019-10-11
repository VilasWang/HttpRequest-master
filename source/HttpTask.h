#ifndef __HTTPTASK_H
#define __HTTPTASK_H
#pragma once

#include "ThreadPool/Task.h"
#include "HttpRequest.h"
#include "HttpRequestDefs.h"

// class HttpTask - Http请求任务
class HttpTask : public TaskBase
{
public:
    explicit HttpTask(bool bAutoDelete = true);
    virtual ~HttpTask();

    void attach(std::shared_ptr<HTTP::IRequest>);
    void detach();
    void exec() override;
    void cancel() override;

private:
    std::shared_ptr<HTTP::IRequest> m_interface;
};

#endif