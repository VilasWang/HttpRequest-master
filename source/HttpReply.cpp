#include "HttpReply.h"
#include "ClassMemoryTracer.h"
#include "HttpManager.h"

namespace {
    struct AsycResultReply
    {
        int id;
        bool success;
        std::string content;
        std::string error_string;
        HTTP::ResultCallback callback;
    };

    struct AsycProgressReply
    {
        int id;
        bool is_download;
        _INT64 total_size;
        _INT64 current_size;
        HTTP::ProgressCallback callback;
    };

    VOID WINAPI APCResultFunc(ULONG_PTR dwParam)
    {
        AsycResultReply *s = reinterpret_cast<AsycResultReply *>(dwParam);
        if (s && s->callback)
        {
            s->callback(s->id, s->success, s->content, s->error_string);
            delete s;
        }
    }

    VOID WINAPI APCProgressFunc(ULONG_PTR dwParam)
    {
        AsycProgressReply *s = reinterpret_cast<AsycProgressReply *>(dwParam);
        if (s && s->callback)
        {
            s->callback(s->id, s->is_download, s->total_size, s->current_size);
            delete s;
        }
    }
}


HttpReply::HttpReply(int requestId)
    : m_result_callback(0)
    , m_progress_callback(0)
    , m_http_code(0)
    , m_request_id(requestId)
    , m_type(HTTP::Unkonwn)
    , m_mode(HTTP::Sync)
{
    TRACE_CLASS_CONSTRUCTOR(HttpReply);
}

HttpReply::~HttpReply()
{
    ///LOG_DEBUG("%s id[%d]\n", __FUNCTION__, m_request_id);
    TRACE_CLASS_DESTRUCTOR(HttpReply);
}

void HttpReply::setRequestType(HTTP::RequestType type)
{
    m_type = type;
}

void HttpReply::setIOMode(HTTP::IOMode mode)
{
    m_mode = mode;
}

void HttpReply::registerResultCallback(HTTP::ResultCallback rc)
{
    m_result_callback = rc;
}

void HttpReply::registerProgressCallback(HTTP::ProgressCallback pc)
{
    m_progress_callback = pc;
}

void HttpReply::replyResult(bool bSuccess)
{
    if (m_result_callback)
    {
        DWORD mainThreadId = HttpManager::globalInstance()->mainThreadId();
        DWORD curThreadId = GetCurrentThreadId();
        if (m_mode == HTTP::Async && mainThreadId != curThreadId)
        {
            AsycResultReply *s = new AsycResultReply;
            s->callback = m_result_callback;
            s->id = m_request_id;
            s->success = (bSuccess && m_http_code == 200);
            s->error_string = m_error_string;

            if (m_type == HTTP::Head)
            {
                s->content = m_receive_header;
            }
            else
            {
                s->content = m_receive_content;
            }

            HANDLE mainThread = OpenThread(THREAD_ALL_ACCESS, TRUE, mainThreadId);
            QueueUserAPC(APCResultFunc, mainThread, ULONG_PTR(s));
            SleepEx(1, TRUE);
            CloseHandle(mainThread);
        }
        else
        {
            if (m_type == HTTP::Head)
            {
                m_result_callback(m_request_id, (bSuccess && m_http_code == 200), m_receive_header, m_error_string);
            }
            else
            {
                m_result_callback(m_request_id, (bSuccess && m_http_code == 200), m_receive_content, m_error_string);
            }
        }
    }
}

void HttpReply::replyProgress(int id, bool is_download, _INT64 total_size, _INT64 current_size)
{
    if (m_progress_callback)
    {
        DWORD mainThreadId = HttpManager::globalInstance()->mainThreadId();
        DWORD curThreadId = GetCurrentThreadId();
        if (m_mode == HTTP::Async && mainThreadId != curThreadId)
        {
            AsycProgressReply *s = new AsycProgressReply;
            s->callback = m_progress_callback;
            s->id = m_request_id;
            s->is_download = is_download;
            s->total_size = total_size;
            s->current_size = current_size;

            HANDLE mainThread = OpenThread(THREAD_ALL_ACCESS, TRUE, mainThreadId);
            QueueUserAPC(APCProgressFunc, mainThread, ULONG_PTR(s));
            SleepEx(1, TRUE);
            CloseHandle(mainThread);
        }
        else
        {
            m_progress_callback(id, is_download, total_size, current_size);
        }
    }
}
