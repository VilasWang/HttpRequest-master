#include <curl/curl.h>
#include "HttpReply.h"
#include "ClassMemoryTracer.h"
#include "log.h"

HttpReply::HttpReply(int requestId)
	: m_result_callback(0)
	, m_progress_callback(0)
	, m_http_code(0)
	, m_id(requestId)
	, m_type(Unkonwn)
{
	TRACE_CLASS_CONSTRUCTOR(HttpReply);
}

HttpReply::~HttpReply()
{
	LOG_DEBUG("%s id[%d]\n", __FUNCTION__, m_id);
	TRACE_CLASS_DESTRUCTOR(HttpReply);
}

void HttpReply::setRequestType(HttpRequestType type)
{
	m_type = type;
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
		if (m_type == Head)
		{
			m_result_callback(m_id, (bSuccess && m_http_code == 200), m_receive_header, m_error_string);
		}
		else
		{
			m_result_callback(m_id, (bSuccess && m_http_code == 200), m_receive_content, m_error_string);
		}
	}
}

void HttpReply::replyProgress(int id, bool is_download, INT64 total_size, INT64 current_size)
{
	if (m_progress_callback)
	{
		m_progress_callback(id, is_download, total_size, current_size);
	}
}
