#ifndef __HTTPREPLY_H
#define __HTTPREPLY_H
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include "HttpRequestDef.h"
#include "HttpRequest_global.h"

// class HttpReply - Http响应类
class HTTP_REQUEST_EXPORT HttpReply
{
public:
    explicit HttpReply(int requestId = 0);
    virtual ~HttpReply();

    int id() const { return m_request_id; }
    long httpStatusCode() const { return m_http_code; }
    std::string readAll() const { return m_receive_content; }
    std::string header() const { return m_receive_header; }
    std::string errorString() const { return m_error_string; }

private:
    void setRequestType(HTTP::RequestType type);
    void setIOMode(HTTP::IOMode mode);
    void setResultCallback(HTTP::ResultCallback rc);
    void setProgressCallback(HTTP::ProgressCallback pc);

    friend class CURLWrapper;

public:
    void replyProgress(int id, bool is_download, INT64 total_size, INT64 current_size);
    void replyResult(bool bSuccess);

private:
    HTTP::ResultCallback m_result_callback;
    HTTP::ProgressCallback m_progress_callback;

    //返回值
    long m_http_code;
    std::string	m_receive_header;
    std::string	m_receive_content;
    std::string	m_error_string;
    int m_request_id;
    HTTP::RequestType m_type;
    HTTP::IOMode m_mode;
};

#endif