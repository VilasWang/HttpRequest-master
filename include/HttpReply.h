#pragma once
#include <windows.h>
#include <string>
#include "httprequestdef.h"
#include "HttpRequest_global.h"

// class HttpReply - Http响应类
class HTTP_REQUEST_EXPORT HttpReply
{
public:
	explicit HttpReply(int requestId = 0);
	~HttpReply();

	int id() const { return m_id; }
	long httpStatusCode() const { return m_http_code; }
	std::string readAll() const { return m_receive_content; }
	std::string header() const { return m_receive_header; }
	std::string errorString() const {return m_error_string;}

public:
	void replyProgress(int id, bool is_download, INT64 total_size, INT64 current_size);

private:
	void replyResult(bool bSuccess);
	void setResultCallback(ResultCallback rc);
	void setProgressCallback(ProgressCallback pc);

	friend class CURLWrapper;

private:
	ResultCallback  m_result_callback;
	ProgressCallback  m_progress_callback;

	//返回值
	long m_http_code;
	std::string	m_receive_content;
	std::string	m_receive_header;
	std::string	m_error_string;
	int m_id;
};

