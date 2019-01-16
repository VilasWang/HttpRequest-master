#include <curl/curl.h>
#include "HttpReply.h"
#include "ClassMemoryTracer.h"

HttpReply::HttpReply(int requestId)
	: m_result_callback(0)
	, m_progress_callback(0)
	, m_http_code(0)
	, m_id(requestId)
{
	TRACE_CLASS_CONSTRUCTOR(HttpReply);
}

HttpReply::~HttpReply()
{
	TRACE_CLASS_DESTRUCTOR(HttpReply);
}

void HttpReply::setResultCallback(ResultCallback rc)
{
	m_result_callback = rc;
}

void HttpReply::setProgressCallback(ProgressCallback pc)
{
	m_progress_callback = pc;
}

void HttpReply::replyResult(bool bSuccess)
{
	if (m_result_callback)
	{
		m_result_callback(m_id, (bSuccess && m_http_code == 200), m_receive_content, m_error_string);
	}
}

void HttpReply::replyProgress(int id, bool is_download, INT64 total_size, INT64 current_size)
{
	if (m_progress_callback)
	{
		m_progress_callback(id, is_download, total_size, current_size);
	}
}
