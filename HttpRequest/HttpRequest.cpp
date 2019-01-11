//#include "stdafx.h"
#include "curl/curl.h"		//libcurl
#include "HttpRequest.h"
#include "HttpTask.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <process.h>
#include "ThreadPool/mutex.h"
#include "ClassMemoryTracer.h"


const int s_nRetryCount = 3;
const int s_kThreadCount = 5;
#define DEFAULT_CONNECT_TIMEOUT						10		//连接最长时间
#define DEFAULT_TIMEOUT								600		//传输最长时间
#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL     3


typedef struct tThreadChunk
{
    FILE* _fp;
    long _startidx;
    long _endidx;
    CURLWrapper* _helper;
} ThreadChunk;


class CURLWrapper : public RequestImpl
{
public:
    CURLWrapper(HttpRequest::RequestType type);
    ~CURLWrapper();

    friend class HttpRequest;

public:
    int	setRetryTimes(int retry_times);
    int	setRequestTimeout(long time_out = 0);
    int	setRequestUrl(const std::string& url);
    int	setFollowLocation(bool follow_location);
    int	setPostData(const char* data, unsigned int size);
    int	setRequestHeader(const std::string& header);
    int	setRequestProxy(const std::string& proxy, long proxy_port);
    int setDownloadFile(const std::string& file_name);
    int setDownloadThreadCount(int thread_count);
    int setUploadFile(const std::string& file_name, const std::string& target_name, const std::string& target_path);

    void setResultCallback(ResultCallback rc);
    void setProgressCallback(ProgressCallback pc);
    void resetCallBacks();

    long getHttpCode()
    {
        return m_http_code;
    }
    bool getHeader(std::string& header);
    bool getContent(std::string& receive);
    bool getErrorString(std::string& error_string);

public:
    int	perform() override;
    void cancel() override;
    int	requestId() override
    {
        return m_id;
    }
    void reset();

private:
    CURLcode publicSetoptMethod(CURL* curl_handle, curl_slist* http_headers);
    int doPostGet();
    int doDownload();
    int doUpload();

    int	download(ThreadChunk* thread_chunk);
    int	splitDownloadCount(INT64 file_size);
    INT64 getDownloadFileSize();

    void defaultResultCallBack(int id, bool success, const std::string& data, const std::string& error);
    void defaultProgressCallback(int id, bool is_download, INT64 total_size, INT64 downloaded_size);

    static UINT WINAPI DownloadProc(LPVOID param);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t write_file_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static int progress_download_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    static int progress_upload_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

private:
    static int s_id_;
    std::shared_ptr<TPLock> m_lock;

    ResultCallback  m_result_callback;
    ProgressCallback  m_progress_callback;

    int m_id;
    HttpRequest::RequestType m_type;
    bool m_is_running;
    bool m_is_cancel;
    bool m_follow_location;
    int m_retry_times;
    long m_time_out;
    long m_proxy_port;
    std::string	m_http_proxy;
    std::string	m_post_data;
    std::string	m_url;
    std::list<std::string> m_list_header;

    //下载
    int	m_thread_count;
    std::string	m_file_path;
    bool m_multi_download;
    bool m_download_failed;
    INT64 m_total_size;
    INT64 m_current_size;

    //上传
    std::string m_strTargetName;
    std::string m_strTargetPath;
    std::string m_strUploadFilePath;

    //返回值
    long m_http_code;
    std::string	m_receive_content;
    std::string	m_receive_header;
    std::string	m_error_string;
};
int CURLWrapper::s_id_ = 0;

//////////////////////////////////////////////////////////////////////////
HttpRequest::HttpRequest(RequestType type)
    : m_request_helper(new CURLWrapper(type))
{
	TRACE_CLASS_CONSTRUCTOR(HttpRequest);
    HttpRequestManager::Instance();
}

HttpRequest::~HttpRequest()
{
	TRACE_CLASS_DESTRUCTOR(HttpRequest);
    m_request_helper = nullptr;
}

int HttpRequest::setRetryTimes(int retry_times)
{
    if (m_request_helper.get())
    {
        return m_request_helper->setRetryTimes(retry_times);
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setRequestTimeout(long time_out)
{
    if (m_request_helper.get())
    {
        return m_request_helper->setRequestTimeout(time_out);
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setRequestUrl(const std::string& url)
{
    if (m_request_helper.get())
    {
        return m_request_helper->setRequestUrl(url);
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setFollowLocation(bool follow_location)
{
    if (m_request_helper.get())
    {
        return m_request_helper->setFollowLocation(follow_location);
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setPostData(const std::string& message)
{
    return setPostData(message.c_str(), message.size());
}

int HttpRequest::setPostData(const char* data, unsigned int size)
{
    if (m_request_helper.get())
    {
        return m_request_helper->setPostData(data, size);
    }
    return REQUEST_INIT_ERROR;
}

int HttpRequest::setRequestHeader(const std::map<std::string, std::string>& headers)
{
    if (m_request_helper.get())
    {
        for (auto it = headers.begin(); it != headers.end(); ++it)
        {
            std::string header = it->first;
            header += ": ";
            header += it->second;
            m_request_helper->setRequestHeader(header);
        }
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setRequestHeader(const std::string& header)
{
    if (m_request_helper.get())
    {
        return m_request_helper->setRequestHeader(header);
    }
    return REQUEST_INIT_ERROR;
}

int HttpRequest::setRequestProxy(const std::string& proxy, long proxy_port)
{
    if (m_request_helper.get())
    {
        return m_request_helper->setRequestProxy(proxy, proxy_port);
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setDownloadFile(const std::string& file_path, int thread_count /* = 5 */)
{
    if (m_request_helper.get())
    {
        m_request_helper->setDownloadFile(file_path);
        m_request_helper->setDownloadThreadCount(thread_count);
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setUploadFile(const std::string& file_path, const std::string& target_name, const std::string& target_path)
{
    if (m_request_helper.get())
    {
        m_request_helper->setUploadFile(file_path, target_name, target_path);
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setResultCallback(ResultCallback rc)
{
    if (m_request_helper.get())
    {
        m_request_helper->setResultCallback(rc);
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

int	HttpRequest::setProgressCallback(ProgressCallback pc)
{
    if (m_request_helper.get())
    {
        m_request_helper->setProgressCallback(pc);
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::perform(CallType type)
{
    int nRequestId = 0;
    if (m_request_helper.get())
    {
        if (!m_request_helper->m_is_running)
        {
            nRequestId = m_request_helper->requestId();
            m_request_helper->reset();

            if (type == Sync)
            {
                m_request_helper->perform();
            }
            else if (type == Async)
            {
                HttpTask* task = new HttpTask(true);
                task->attach(m_request_helper);
                HttpRequestManager::addTask(task);
            }
        }
    }

    return nRequestId;
}

bool HttpRequest::cancel(int requestId)
{
    return HttpRequestManager::abortTask(requestId);
}

bool HttpRequest::cancelAll()
{
	return HttpRequestManager::abortAllTask();
}

void HttpRequest::globalCleanup()
{
	return HttpRequestManager::globalCleanup();
}

bool HttpRequest::getHttpCode(long& http_code)
{
    if (nullptr == m_request_helper.get())
    {
        return false;
    }

    http_code = m_request_helper->getHttpCode();
    return true;
}

bool HttpRequest::getReceiveHeader(std::string& header)
{
    if (nullptr == m_request_helper.get())
    {
        return false;
    }

    return m_request_helper->getHeader(header);
}

bool HttpRequest::getReceiveContent(std::string& receive)
{
    if (nullptr == m_request_helper.get())
    {
        return false;
    }

    return m_request_helper->getContent(receive);
}

bool HttpRequest::getErrorString(std::string& error_string)
{
    if (nullptr == m_request_helper.get())
    {
        return false;
    }

    return m_request_helper->getErrorString(error_string);
}

//////////////////////////////////////////////////////////////////////////
CURLWrapper::CURLWrapper(HttpRequest::RequestType type)
    : m_type(type)
    , m_id(++s_id_)
    , m_is_running(false)
    , m_is_cancel(false)
    , m_follow_location(false)
    , m_retry_times(s_nRetryCount)
    , m_time_out(0)
    , m_proxy_port(0)
    , m_thread_count(0)
    , m_multi_download(false)
    , m_download_failed(false)
    , m_total_size(0)
    , m_current_size(0)
    , m_http_code(0)
    , m_lock(new TPLock)
{
	TRACE_CLASS_CONSTRUCTOR(CURLWrapper);
    setResultCallback(std::bind(&CURLWrapper::defaultResultCallBack, this,
                                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

    setProgressCallback(std::bind(&CURLWrapper::defaultProgressCallback, this,
                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
}

CURLWrapper::~CURLWrapper()
{
    //char ch[64];
    //sprintf_s(ch, "%s id:%d\n", __FUNCTION__, m_id);
    //OutputDebugStringA(ch);
	TRACE_CLASS_DESTRUCTOR(CURLWrapper);

    cancel();
    resetCallBacks();
    m_lock = nullptr;
}

int	CURLWrapper::setRetryTimes(int retry_times)
{
    m_retry_times = retry_times;
    return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setRequestTimeout(long time_out)
{
    if (time_out < 0)
    {
        return HttpRequest::REQUEST_INVALID_OPT;
    }
    m_time_out = time_out;
    return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setRequestUrl(const std::string& url)
{
    m_url = url;
    return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setFollowLocation(bool follow_location)
{
    m_follow_location = follow_location;
    return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setPostData(const char* data, unsigned int size)
{
    m_post_data = std::string(data, size);
    return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setRequestHeader(const std::string& header)
{
    if (header.empty())
    {
        return HttpRequest::REQUEST_INVALID_OPT;
    }

    m_list_header.push_back(header);
    return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setRequestProxy(const std::string& proxy, long proxy_port)
{
    m_http_proxy = proxy;
    m_proxy_port = proxy_port;
    return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setDownloadFile(const std::string& file_name)
{
    m_file_path = file_name;
    return CURLE_OK;
}

int CURLWrapper::setDownloadThreadCount(int thread_count)
{
    m_thread_count = thread_count;
    return CURLE_OK;
}

int CURLWrapper::setUploadFile(const std::string& file_path, const std::string& target_name, const std::string& target_path)
{
    m_strUploadFilePath = file_path;
    m_strTargetName = target_name;
    m_strTargetPath = target_path;
    return CURLE_OK;
}

void CURLWrapper::setResultCallback(ResultCallback rc)
{
    TPLocker locker(m_lock);
    m_result_callback = rc;
}

void CURLWrapper::setProgressCallback(ProgressCallback pc)
{
    TPLocker locker(m_lock);
    m_progress_callback = pc;
}

void CURLWrapper::resetCallBacks()
{
    TPLocker locker(m_lock);
    m_result_callback = nullptr;
    m_progress_callback = nullptr;
}

void CURLWrapper::defaultResultCallBack(int id, bool success, const std::string& data, const std::string&)
{
    //default result callback do nothing
}

void CURLWrapper::defaultProgressCallback(int id, bool is_download, INT64 total_size, INT64 downloaded_size)
{
    //default download callback do nothing
}

CURLcode CURLWrapper::publicSetoptMethod(CURL* curl_handle, curl_slist* http_headers)
{
    CURLcode curl_code = CURLE_FAILED_INIT;
    if (curl_handle)
    {
        if (m_url.substr(0, 5) == "https")
        {
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        curl_code = curl_easy_setopt(curl_handle, CURLOPT_URL, m_url.c_str());

        if (m_follow_location)
        {
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5);
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
        }
        else
        {
            curl_code =  curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 0L);
        }

        curl_code = curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
        curl_code = curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, DEFAULT_CONNECT_TIMEOUT);
        curl_code = curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, m_time_out);
    }

    if (!m_list_header.empty())
    {
        for (auto iter = m_list_header.cbegin(); iter != m_list_header.cend(); ++iter)
        {
            http_headers = curl_slist_append(http_headers, (*iter).c_str());
        }

        if (http_headers)
        {
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, http_headers);
            if (curl_code != CURLE_OK)
            {
                return curl_code;
            }
        }
    }

    return curl_code;
}

int CURLWrapper::perform()
{
    int curl_code = CURLE_UNSUPPORTED_PROTOCOL;
    if (m_is_cancel)
    {
        return curl_code;
    }

    m_is_running = true;
    m_receive_header.clear();
    m_receive_content.clear();
    m_error_string.clear();
    m_http_code = 0;

    if (m_type == HttpRequest::Post || m_type == HttpRequest::Get)
    {
        curl_code = doPostGet();
    }
    else if (m_type == HttpRequest::Download)
    {
        curl_code = doDownload();
    }
    else if (m_type == HttpRequest::Upload)
    {
        curl_code = doUpload();
    }

    if (!m_is_cancel)
    {
        m_lock->lock();
        if (m_result_callback)
        {
            bool success = (curl_code == CURLE_OK && m_http_code == 200);
            if (m_type == HttpRequest::Download)
            {
                success = (curl_code == CURLE_OK && (m_http_code >= 200 && m_http_code < 300));
            }
            m_result_callback(m_id, success, m_receive_content, m_error_string);
        }
        m_lock->unLock();
    }
    m_is_running = false;

    return curl_code;
}

void CURLWrapper::cancel()
{
    m_is_cancel = true;
}

int CURLWrapper::doPostGet()
{
    CURL* curl_handle = curl_easy_init();
    if (curl_handle)
    {
        HttpRequestManager::set_share_handle(curl_handle);

        curl_slist*	http_headers = nullptr;
        CURLcode curl_code = publicSetoptMethod(curl_handle, http_headers);
        if (curl_code != CURLE_OK)
        {
            curl_slist_free_all(http_headers);
            curl_easy_cleanup(curl_handle);
            return curl_code;
        }

        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, CURLWrapper::header_callback);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &m_receive_header);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, CURLWrapper::write_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &m_receive_content);
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1);

        if (m_type == HttpRequest::Post)
        {
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
            if (curl_code == CURLE_OK)
            {
                if (m_post_data.size() == 0)
                {
                    curl_code = curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, "");
                }
                else
                {
                    curl_code = curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, m_post_data.c_str());
                }

                if (curl_code == CURLE_OK)
                {
                    curl_code = curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, m_post_data.size());
                }
            }
        }

        curl_code = curl_easy_perform(curl_handle);
        if (curl_code == CURLE_OPERATION_TIMEDOUT)
        {
            int retry_count = m_retry_times;
            while (!m_is_cancel && retry_count > 0)
            {
                curl_code = curl_easy_perform(curl_handle);
                if (curl_code != CURLE_OPERATION_TIMEDOUT)
                {
                    break;
                }
                retry_count--;
            }
        }

        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &m_http_code);
        if (curl_code != CURLE_OK || m_http_code != 200)
        {
            const char* err_string = curl_easy_strerror(curl_code);
            m_error_string = err_string;
        }

        curl_slist_free_all(http_headers);
        curl_easy_cleanup(curl_handle);
        return curl_code;
    }

    return CURLE_FAILED_INIT;
}

int CURLWrapper::doDownload()
{
    m_total_size = getDownloadFileSize();
    if (m_total_size < 0)
    {
        m_multi_download = false;
    }

    std::string file_name = m_file_path;
    std::string out_file_name = file_name + ".dl";
    DeleteFileA(out_file_name.c_str());

    FILE* fp = nullptr;
    fopen_s(&fp, out_file_name.c_str(), "wb+");
    if (nullptr == fp)
    {
        m_error_string = "Open local file error!";
        return HttpRequest::REQUEST_OPENFILE_ERROR;
    }

    int ret_code = HttpRequest::REQUEST_PERFORM_ERROR;
    if (m_multi_download)
    {
        int thread_count = splitDownloadCount(m_total_size);
        m_thread_count = thread_count > m_thread_count ? m_thread_count : thread_count;
        if (m_thread_count <= 1)
        {
            m_multi_download = false;
        }
    }
    char ch[64];
    sprintf_s(ch, "multi-download: %d; thread-count: %d\n", m_multi_download, m_thread_count);
    OutputDebugStringA(ch);

    //文件大小有分开下载的必要并且服务器支持多线程下载时，启用多线程下载
    if (m_multi_download)
    {
        long gap = static_cast<long>(m_total_size) / m_thread_count;
        std::vector<HANDLE> threads;

        for (int i = 0; i < m_thread_count; i++)
        {
            ThreadChunk* thread_chunk = new ThreadChunk;
            thread_chunk->_fp = fp;
            thread_chunk->_helper = this;

            if (i < m_thread_count - 1)
            {
                thread_chunk->_startidx = i * gap;
                thread_chunk->_endidx = thread_chunk->_startidx + gap - 1;
            }
            else
            {
                thread_chunk->_startidx = i * gap;
                thread_chunk->_endidx = -1;
            }

            UINT thread_id;
            HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, &CURLWrapper::DownloadProc, thread_chunk, 0, &thread_id);
            threads.push_back(hThread);
        }

        WaitForMultipleObjects(threads.size(), &threads[0], TRUE, INFINITE);
        for (int i = 0; i < threads.size(); ++i)
        {
            HANDLE handle = threads[i];
            CloseHandle(handle);
        }
    }
    else
    {
        ThreadChunk* thread_chunk = new ThreadChunk;
        thread_chunk->_fp = fp;
        thread_chunk->_helper = this;
        thread_chunk->_startidx = 0;
        thread_chunk->_endidx = 0;
        download(thread_chunk);
    }

    fclose(fp);

    if (m_download_failed == false)
    {
        ret_code = HttpRequest::REQUEST_OK;
        MoveFileExA(out_file_name.c_str(), file_name.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
    else
    {
        DeleteFileA(out_file_name.c_str());
    }

    return ret_code;
}

int CURLWrapper::doUpload()
{
    CURL* curl_handle = curl_easy_init();
    if (curl_handle)
    {
        HttpRequestManager::set_share_handle(curl_handle);

        curl_slist*	http_headers = nullptr;
        CURLcode curl_code = publicSetoptMethod(curl_handle, http_headers);
        if (curl_code != CURLE_OK)
        {
            const char* err_string = curl_easy_strerror(curl_code);
            m_error_string = err_string;
            curl_slist_free_all(http_headers);
            curl_easy_cleanup(curl_handle);
            return curl_code;
        }

        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, CURLWrapper::header_callback);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, nullptr);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, CURLWrapper::write_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &m_receive_content);
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, CURLWrapper::progress_upload_callback);
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, this);

        struct curl_httppost* httppost = NULL;
        struct curl_httppost* lastpost = NULL;
        /*
        Fill in the file upload field. This makes libcurl load data from
        the given file name when curl_easy_perform() is called.
        */
        curl_formadd(&httppost,
                     &lastpost,
                     CURLFORM_COPYNAME, "sendfile",
                     CURLFORM_FILE, m_strUploadFilePath.c_str(),
                     CURLFORM_END);

        /* Fill in the filename field */
        curl_formadd(&httppost,
                     &lastpost,
                     CURLFORM_COPYNAME, "filename",
                     CURLFORM_COPYCONTENTS, m_strTargetName.c_str(),
                     CURLFORM_END);

        /* Fill in the path field */
        curl_formadd(&httppost,
                     &lastpost,
                     CURLFORM_COPYNAME, "path",
                     CURLFORM_COPYCONTENTS, m_strTargetPath.c_str(),
                     CURLFORM_END);

        /* Fill in the submit field too, even if this is rarely needed */
        curl_formadd(&httppost,
                     &lastpost,
                     CURLFORM_COPYNAME, "submit",
                     CURLFORM_COPYCONTENTS, "send",
                     CURLFORM_END);

        curl_easy_setopt(curl_handle, CURLOPT_HTTPPOST, httppost);

        curl_code = curl_easy_perform(curl_handle);
        if (curl_code == CURLE_OPERATION_TIMEDOUT)
        {
            int retry_count = m_retry_times;
            while (!m_is_cancel && retry_count > 0)
            {
                curl_code = curl_easy_perform(curl_handle);
                if (curl_code != CURLE_OPERATION_TIMEDOUT)
                {
                    break;
                }
                retry_count--;
            }
        }

        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &m_http_code);
        if (curl_code != CURLE_OK || m_http_code != 200)
        {
            const char* err_string = curl_easy_strerror(curl_code);
            m_error_string = err_string;
        }

        if (httppost)
        {
            curl_formfree(httppost);
        }
        if (http_headers)
        {
            curl_slist_free_all(http_headers);
        }
        curl_easy_cleanup(curl_handle);
        return curl_code;
    }

    return CURLE_FAILED_INIT;
}

int CURLWrapper::download(ThreadChunk* thread_chunk)
{
    CURLcode curl_code = CURLE_FAILED_INIT;
    CURL* curl_handle = curl_easy_init();
    if (curl_handle)
    {
        HttpRequestManager::set_share_handle(curl_handle);

        curl_slist*	http_headers = nullptr;
        CURLcode curl_code = publicSetoptMethod(curl_handle, http_headers);
        if (curl_code != CURLE_OK)
        {
            const char* err_string = curl_easy_strerror(curl_code);
            m_error_string = err_string;
            m_download_failed = true;
            curl_slist_free_all(http_headers);
            curl_easy_cleanup(curl_handle);
            return curl_code;
        }

        curl_easy_setopt(curl_handle, CURLOPT_POST, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, CURLWrapper::header_callback);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, nullptr);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, CURLWrapper::write_file_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, thread_chunk);
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, CURLWrapper::progress_download_callback);
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, thread_chunk);
        curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_TIME, 5L);

        //const char* user_agent = ("Mozilla/5.0 (Windows NT 5.1; rv:5.0) Gecko/20100101 Firefox/5.0");
        //curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, user_agent);

        if (thread_chunk && thread_chunk->_endidx != 0)
        {
            std::string range;
            std::ostringstream ostr;
            if (thread_chunk->_endidx > 0)
            {
                ostr << thread_chunk->_startidx << "-" << thread_chunk->_endidx;
            }
            else
            {
                ostr << thread_chunk->_startidx << "-";
            }

            range = ostr.str();
            curl_easy_setopt(curl_handle, CURLOPT_RANGE, range.c_str());
        }

        curl_code = curl_easy_perform(curl_handle);
        if (curl_code == CURLE_OPERATION_TIMEDOUT)
        {
            int retry_count = m_retry_times;
            while (!m_download_failed && retry_count > 0)
            {
                curl_code = curl_easy_perform(curl_handle);
                if (curl_code != CURLE_OPERATION_TIMEDOUT)
                {
                    break;
                }
                retry_count--;
            }
        }

        bool chunk_download_fail = false;
        long http_code = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
        if (curl_code == CURLE_OK && (http_code >= 200 && http_code <= 300))
        {
            chunk_download_fail = false;
            if (m_http_code == 0)
            {
                m_http_code = http_code;
            }
        }
        else
        {
			if (curl_code != CURLE_OK)
			{
				const char* err_string = curl_easy_strerror(curl_code);
				m_error_string = err_string;
			}
			else
			{
				char ch[32];
				sprintf_s(ch, "http code[%d]", http_code);
				m_error_string = ch;
			}
            chunk_download_fail = true;
        }
        if (chunk_download_fail)
        {
            m_download_failed = true;
            m_http_code = http_code;
        }

        curl_slist_free_all(http_headers);
        curl_easy_cleanup(curl_handle);
    }

    if (thread_chunk)
    {
        delete thread_chunk;
    }

    return curl_code;
}

INT64 CURLWrapper::getDownloadFileSize()
{
    if (m_url.empty())
    {
        return -1;
    }
    else
    {
        INT64 file_size = -1;
        CURL* curl_handle = curl_easy_init();
        if (curl_handle)
        {
            HttpRequestManager::set_share_handle(curl_handle);

            curl_slist*	http_headers = nullptr;
            CURLcode curl_code = publicSetoptMethod(curl_handle, http_headers);
            if (curl_code != CURLE_OK)
            {
                const char* err_string = curl_easy_strerror(curl_code);
                m_error_string = err_string;
                m_download_failed = true;
                curl_slist_free_all(http_headers);
                curl_easy_cleanup(curl_handle);
                return -1;
            }

            curl_easy_setopt(curl_handle, CURLOPT_HEADER, 1);
            curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1);
            curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, CURLWrapper::header_callback);
            curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &m_receive_header);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, CURLWrapper::write_callback);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, nullptr);
            curl_easy_setopt(curl_handle, CURLOPT_RANGE, "2-");

            curl_code = curl_easy_perform(curl_handle);
            curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &m_http_code);
            if (curl_code == CURLE_OK && (m_http_code >= 200 && m_http_code < 300))
            {
				double size = -1.0;
                curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);
				file_size = size;

                //匹配"Content-Range: bytes 2-1449/26620" 则证明支持多线程下载
                std::regex pattern("CONTENT-RANGE\\s*:\\s*\\w+\\s*(\\d+)-(\\d*)/(\\d+)", std::regex::icase);
                m_multi_download = std::regex_search(m_receive_header, pattern);
            }
            else
            {
                const char* err_string = curl_easy_strerror(curl_code);
                m_error_string = err_string;
            }

            curl_easy_cleanup(curl_handle);
        }

        return file_size;
    }
}

int CURLWrapper::splitDownloadCount(INT64 total_size)
{
    const INT64 size_2MB = 2 * 1024 * 1024;
    const INT64 size_10MB = 10 * 1024 * 1024;
    const INT64 size_50MB = 50 * 1024 * 1024;

    int max_count = s_kThreadCount * 2;

    if (total_size <= size_2MB)
    {
        return 1;
    }
    else if (total_size > size_2MB && total_size <= size_10MB)
    {
        return static_cast<int>(total_size / (size_2MB));
    }
    else if (total_size > size_10MB && total_size <= size_50MB)
    {
        return s_kThreadCount;
    }
    else
    {
        int count = static_cast<int>(total_size / size_10MB);
        return count > max_count ? max_count : count;
    }

    return 1;
}

void CURLWrapper::reset()
{
    if (m_is_running)
    {
        return;
    }

    m_multi_download = false;
    m_download_failed = false;
    m_is_running = false;
    m_is_cancel = false;
    m_thread_count = 1;
    m_total_size = 0.0;
    m_current_size = 0.0;
    m_http_code = 0;
    m_receive_header.clear();
    m_error_string.clear();
    m_receive_header.clear();
}

bool CURLWrapper::getHeader(std::string& header)
{
    if (m_receive_header.empty())
    {
        return false;
    }

    header = m_receive_header;
    return true;
}

bool CURLWrapper::getContent(std::string& receive)
{
    if (m_receive_content.empty())
    {
        return false;
    }

    receive = m_receive_content;
    return true;
}

bool CURLWrapper::getErrorString(std::string& error_string)
{
    if (m_error_string.empty())
    {
        return false;
    }

    error_string = m_error_string;
    return true;
}

UINT WINAPI CURLWrapper::DownloadProc(LPVOID param)
{
    ThreadChunk* thread_chunk = reinterpret_cast<ThreadChunk*>(param);
    if (thread_chunk && thread_chunk->_helper)
    {
        return thread_chunk->_helper->download(thread_chunk);
    }
    return -1;
}

size_t CURLWrapper::header_callback(char* buffer, size_t size, size_t nitems, void* userdata)
{
    std::string* receive_header = reinterpret_cast<std::string*>(userdata);
    if (receive_header && buffer)
    {
        receive_header->append(reinterpret_cast<const char*>(buffer), size * nitems);
    }

    return nitems * size;
}

size_t CURLWrapper::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* receive_content = reinterpret_cast<std::string*>(userdata);
    if (receive_content && ptr)
    {
        receive_content->append(reinterpret_cast<const char*>(ptr), size * nmemb);
    }

    return nmemb * size;
}

size_t CURLWrapper::write_file_callback(char* buffer, size_t size, size_t nmemb, void* userdata)
{
    ThreadChunk* thread_chunk = reinterpret_cast<ThreadChunk*>(userdata);
    CURLWrapper* helper = nullptr;
    if (thread_chunk)
    {
        helper = thread_chunk->_helper;
    }

    if (!thread_chunk || !helper || helper->m_is_cancel || helper->m_download_failed)
    {
        return CURL_WRITEFUNC_PAUSE;
    }

    size_t written = 0;
    helper->m_lock->lock();
    int seek_error = fseek(thread_chunk->_fp, thread_chunk->_startidx, SEEK_SET);
    if (seek_error != 0)
    {
        OutputDebugStringA("fseek error!");
    }
    else
    {
        written = fwrite(buffer, size, nmemb, thread_chunk->_fp);
    }
    thread_chunk->_startidx += written;
    helper->m_current_size += written;
    helper->m_lock->unLock();

    return written;
}

int CURLWrapper::progress_download_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    ThreadChunk* thread_chunk = reinterpret_cast<ThreadChunk*>(clientp);
    CURLWrapper* helper = nullptr;
    if (thread_chunk)
    {
        helper = thread_chunk->_helper;
    }

    if (!thread_chunk || !helper || helper->m_is_cancel || helper->m_download_failed)
    {
        return -1;
    }

    if (dltotal > 0 && dlnow > 0 && helper->m_progress_callback)
    {
        //CMutexLocker http_lock(downloader->m_lock);
        if (helper->m_multi_download)
        {
            helper->m_progress_callback(helper->m_id, true, helper->m_total_size, helper->m_current_size);
        }
        else
        {
            helper->m_progress_callback(helper->m_id, true, dltotal, dlnow);
        }
    }

    return 0;
}

int CURLWrapper::progress_upload_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    CURLWrapper* helper = reinterpret_cast<CURLWrapper*>(clientp);
    if (!helper || helper->m_is_cancel)
    {
        return -1;
    }

    if (dlnow > 0 && helper->m_progress_callback)
    {
        //CMutexLocker http_lock(helper->m_lock);
        helper->m_progress_callback(helper->m_id, false, ultotal, ulnow);
    }

    return 0;
}