﻿#include "HttpRequest.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <process.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <list>
#include "Lock.h"
#include "Log.h"
#include "ClassMemoryTracer.h"
#include "HttpTask.h"
#include "HttpManager.h"
#include "HttpReply.h"

using namespace VCUtil;

#define DEFAULT_RETRY_COUNT					3
#define DEFAULT_DOWNLOAD_THREAD_COUNT		5
#define DEFAULT_CONNECT_TIMEOUT				10		//连接最长时间
#define DEFAULT_TIMEOUT						600		//传输最长时间
#define MINIMAL_PROGRESS_INTERVAL			3

struct DownloadChunk
{
    CURLWrapper* _request;
    FILE* _fp;
    long _startidx;
    long _endidx;

    DownloadChunk() : _request(nullptr), _fp(nullptr), _startidx(0), _endidx(0)
    {
        TRACE_CLASS_CONSTRUCTOR(DownloadChunk);
    }
    virtual ~DownloadChunk()
    {
        TRACE_CLASS_DESTRUCTOR(DownloadChunk);
    }
};

struct UploadChannel
{
    CURLWrapper* _request;
    FILE* _fp;

    UploadChannel() : _request(nullptr), _fp(nullptr)
    {
        TRACE_CLASS_CONSTRUCTOR(UploadChannel);
    }
    virtual ~UploadChannel()
    {
        TRACE_CLASS_DESTRUCTOR(UploadChannel);
    }
};

class CURLWrapper : public HTTP::IRequest
{
public:
    CURLWrapper();
    virtual ~CURLWrapper();

    friend class HttpRequest;

public:
    int	setRetryTimes(int retry_times);
    int	setTimeout(long time_out = 0);
    int	setUrl(const std::string& url);
    int	setFollowLocation(bool follow_location);
    int	setPostData(const char* data, unsigned int size);
    int	setHeader(const std::string& header);
    int	setProxy(const std::string& proxy, long proxy_port);
    int setDownloadFile(const std::string& file_name);
    int setDownloadThreadCount(int thread_count);
    //HTTP put 方式上次文件
    int setUploadFile(const std::string& file_name);
    //HTTP Multipart formpost 方式上次文件
    int setUploadFile(const std::string& file_name, const std::string& target_name, const std::string& target_path);
    void registerResultCallback(HTTP::ResultCallback rc);
    void registerProgressCallback(HTTP::ProgressCallback pc);
    void setRequestType(HTTP::RequestType);
    void setIOMode(HTTP::IOMode mode);

public:
    int	perform() override;
    void cancel() override;
    int	requestId() override { return m_id; }

    bool isRunning() const override;
    bool isCanceled() const override;
    bool isFailed() const override;

    void setRunning(bool bRunning);
    void setCanceled(bool bCancel);
    void setFailed(bool bFail);

    _INT64 totalBytes() const { return m_total_size; }
    _INT64 currentBytes() const;
    void setCurrentBytes(_INT64);

    bool isMultiDownload() const;

    void reset();

private:
    CURLcode publicSetoptMethod(CURL* curl_handle, curl_slist* http_headers);
    int doPostGet();
    int doDownload();
    int doUpload();
    int doFormPostUpload();//实际上是执行curl httppost的方式
    int doHead();

    int	download(DownloadChunk* download_chunk);
    int	splitDownloadCount(_INT64 file_size);
    _INT64 getDownloadFileSize();

    static UINT WINAPI downloadProc(LPVOID param);

private:
    mutable CSLock m_lock;

    int m_id;
    HTTP::RequestType m_type;
    HTTP::IOMode m_mode;

#if defined(_MSC_VER) && _MSC_VER < 1700
    static int ms_id;
    bool m_is_running;
    bool m_is_cancel;
    bool m_is_failed;
#else
    static std::atomic<int> ms_id;
    std::atomic<bool> m_is_running;
    std::atomic<bool> m_is_cancel;
    std::atomic<bool> m_is_failed;
#endif
    bool m_follow_location;
    int m_retry_times;
    long m_time_out;
    long m_proxy_port;
    std::string	m_http_proxy;
    std::string	m_post_data;
    std::string	m_url;
    std::list<std::string> m_list_header;
    HTTP::ResultCallback  m_result_callback;
    HTTP::ProgressCallback  m_progress_callback;

    //下载
    std::string	m_file_path;
    int	m_thread_count;
#if !defined(_MSC_VER) || _MSC_VER >= 1700
    std::atomic<bool> m_multi_download;
    std::atomic<_INT64> m_total_size;
    std::atomic<_INT64> m_current_size;
#else
    bool m_multi_download;
    _INT64 m_total_size;
    _INT64 m_current_size;
#endif

    //上传
    std::string m_strTargetName;
    std::string m_strTargetPath;
    std::string m_strUploadFile;
};
#if !defined(_MSC_VER) || _MSC_VER >= 1700
std::atomic<int> CURLWrapper::ms_id = 0;
#else
int CURLWrapper::ms_id = 0;
#endif

namespace {
    size_t header_callback(char* buffer, size_t size, size_t nmemb, void* userdata)
    {
        std::string* receive_header = reinterpret_cast<std::string*>(userdata);
        if (nullptr == receive_header || nullptr == buffer)
            return 0;

        receive_header->append(reinterpret_cast<const char*>(buffer), size * nmemb);
        return nmemb * size;
    }

    size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
    {
        std::string* receive_content = reinterpret_cast<std::string*>(userdata);
        if (nullptr == receive_content || nullptr == ptr)
            return 0;

        receive_content->append(reinterpret_cast<const char*>(ptr), size * nmemb);
        return nmemb * size;
    }

    size_t read_file_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
    {
        UploadChannel *uc = reinterpret_cast<UploadChannel *>(userdata);
        if (!uc || !uc->_fp || !uc->_request || uc->_request->isFailed())
            return 0;

        if (uc->_request->isCanceled())
            return CURL_READFUNC_ABORT;

        size_t retcode = fread(ptr, size, nmemb, (FILE *)uc->_fp);
        size_t nread = retcode * size;

        fprintf_s(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T
            " bytes from file\n", (curl_off_t)nread);

        return nread;
    }

    size_t write_file_callback(char* buffer, size_t size, size_t nmemb, void* userdata)
    {
        CURLWrapper* pRequest = nullptr;
        DownloadChunk* download_chunk = reinterpret_cast<DownloadChunk*>(userdata);
        if (download_chunk)
        {
            pRequest = download_chunk->_request;
        }

        if (!download_chunk || !pRequest || pRequest->isCanceled() || pRequest->isFailed())
            return 0;

        size_t written = 0;
        if (0 == fseek(download_chunk->_fp, download_chunk->_startidx, SEEK_SET))
        {
            written = fwrite(buffer, size, nmemb, download_chunk->_fp);
            if (written > 0)
            {
                download_chunk->_startidx += written;
                pRequest->setCurrentBytes(pRequest->currentBytes() + written);
            }
        }
        else
        {
            LOG_DEBUG("[HttpRequest] %s - fseek error! [%ul]\n", __FUNCTION__, GetLastError());
        }

        return written;
    }

    int progress_download_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
    {
        CURLWrapper* pRequest = nullptr;
        DownloadChunk* download_chunk = reinterpret_cast<DownloadChunk*>(clientp);
        if (download_chunk)
        {
            pRequest = download_chunk->_request;
        }

        if (!download_chunk || !pRequest || pRequest->isCanceled() || pRequest->isFailed())
            return -1;

        std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(pRequest->requestId());
        if (dltotal > 0 && dlnow > 0 && reply.get())
        {
            if (pRequest->isMultiDownload())
            {
                reply->replyProgress(pRequest->requestId(), true, pRequest->totalBytes(), pRequest->currentBytes());
            }
            else
            {
                reply->replyProgress(pRequest->requestId(), true, dltotal, dlnow);
            }
        }

        return 0;
    }

    int progress_upload_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
    {
        HTTP::IRequest* pRequest = reinterpret_cast<HTTP::IRequest*>(clientp);
        if (!pRequest || pRequest->isCanceled() || pRequest->isFailed())
            return -1;

        std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(pRequest->requestId());
        if (ultotal > 0 && ulnow > 0 && reply.get())
        {
            reply->replyProgress(pRequest->requestId(), false, ultotal, ulnow);
        }

        return 0;
    }
}

CURLWrapper::CURLWrapper()
    : m_type(HTTP::Unkonwn)
    , m_id(++ms_id)
    , m_is_running(false)
    , m_is_cancel(false)
    , m_is_failed(false)
    , m_follow_location(true)
    , m_retry_times(DEFAULT_RETRY_COUNT)
    , m_time_out(0)
    , m_proxy_port(0)
    , m_thread_count(0)
    , m_multi_download(true)
    , m_total_size(0)
    , m_current_size(0)
    , m_progress_callback(nullptr)
    , m_result_callback(nullptr)
{
    TRACE_CLASS_CONSTRUCTOR(CURLWrapper);
}

CURLWrapper::~CURLWrapper()
{
    ///LOG_DEBUG("%s id[%d]\n", __FUNCTION__, m_id);
    TRACE_CLASS_DESTRUCTOR(CURLWrapper);
    cancel();
}

int	CURLWrapper::setRetryTimes(int retry_times)
{
    m_retry_times = retry_times;
    return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setTimeout(long time_out)
{
    if (time_out < 0)
    {
        return HttpRequest::REQUEST_INVALID_OPT;
    }
    m_time_out = time_out;
    return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setUrl(const std::string& url)
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

int CURLWrapper::setHeader(const std::string& header)
{
    if (header.empty())
    {
        return HttpRequest::REQUEST_INVALID_OPT;
    }

    m_list_header.push_back(header);
    return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setProxy(const std::string& proxy, long proxy_port)
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

int CURLWrapper::setUploadFile(const std::string& file_path)
{
    m_strUploadFile = file_path;
    return CURLE_OK;
}

int CURLWrapper::setUploadFile(const std::string& file_path, const std::string& target_name, const std::string& target_path)
{
    m_strUploadFile = file_path;
    m_strTargetName = target_name;
    m_strTargetPath = target_path;
    return CURLE_OK;
}

void CURLWrapper::registerResultCallback(HTTP::ResultCallback rc)
{
    m_result_callback = rc;
}

void CURLWrapper::registerProgressCallback(HTTP::ProgressCallback pc)
{
    m_progress_callback = pc;
}

void CURLWrapper::setRequestType(HTTP::RequestType type)
{
    m_type = type;
}

void CURLWrapper::setIOMode(HTTP::IOMode mode)
{
    m_mode = mode;
}

CURLcode CURLWrapper::publicSetoptMethod(CURL* curl_handle, curl_slist* http_headers)
{
    CURLcode curl_code = CURLE_FAILED_INIT;
    if (curl_handle)
    {
        if (m_url.substr(0, 5) == "https" || m_url.substr(0, 5) == "HTTPS")
        {
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        curl_code = curl_easy_setopt(curl_handle, CURLOPT_URL, m_url.c_str());

        if (m_follow_location)
        {
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5);
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_AUTOREFERER, 1L);
        }
        else
        {
            curl_code = curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 0L);
        }

        curl_code = curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
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
        }
    }

    return curl_code;
}

bool CURLWrapper::isRunning() const
{
#if defined(_MSC_VER) && _MSC_VER < 1700
    Locker<CSLock> locker(m_lock);
#endif
    return m_is_running;
}

void CURLWrapper::setRunning(bool bRunning)
{
#if defined(_MSC_VER) && _MSC_VER < 1700
    Locker<CSLock> locker(m_lock);
#endif
    m_is_running = bRunning;
}

bool CURLWrapper::isCanceled() const
{
#if defined(_MSC_VER) && _MSC_VER < 1700
    Locker<CSLock> locker(m_lock);
#endif
    return m_is_cancel;
}

void CURLWrapper::setCanceled(bool bCancel)
{
#if defined(_MSC_VER) && _MSC_VER < 1700
    Locker<CSLock> locker(m_lock);
#endif
    m_is_cancel = bCancel;
}

bool CURLWrapper::isFailed() const
{
#if defined(_MSC_VER) && _MSC_VER < 1700
    Locker<CSLock> locker(m_lock);
#endif
    return m_is_failed;
}

void CURLWrapper::setFailed(bool bFailed)
{
#if defined(_MSC_VER) && _MSC_VER < 1700
    Locker<CSLock> locker(m_lock);
#endif
    m_is_failed = bFailed;
}

_INT64 CURLWrapper::currentBytes() const
{
#if defined(_MSC_VER) && _MSC_VER < 1700
    Locker<CSLock> locker(m_lock);
#endif
    return m_current_size;
}

void CURLWrapper::setCurrentBytes(_INT64 current_size)
{
#if defined(_MSC_VER) && _MSC_VER < 1700
    Locker<CSLock> locker(m_lock);
#endif
    m_current_size = current_size;
}

bool CURLWrapper::isMultiDownload() const
{
#if defined(_MSC_VER) && _MSC_VER < 1700
    Locker<CSLock> locker(m_lock);
#endif
    return m_multi_download;
}

int CURLWrapper::perform()
{
    int curl_code = CURLE_UNKNOWN_OPTION;
    if (isCanceled())
    {
        return curl_code;
    }

    setRunning(true);

    std::shared_ptr<HttpReply> rly = HttpManager::globalInstance()->getReply(m_id);
    if (!rly.get())
    {
        setRunning(false);
        curl_code = CURLE_FAILED_INIT;
        return curl_code;
    }
    rly->setRequestType(m_type);
    rly->setIOMode(m_mode);
    rly->registerProgressCallback(m_progress_callback);
    rly->registerResultCallback(m_result_callback);

    if (m_type == HTTP::Post || m_type == HTTP::Get)
    {
        curl_code = doPostGet();
    }
    else if (m_type == HTTP::Download)
    {
        curl_code = doDownload();
    }
    else if (m_type == HTTP::Upload)
    {
        curl_code = doUpload();
    }
    else if (m_type == HTTP::Upload2)
    {
        curl_code = doFormPostUpload();
    }
    else if (m_type == HTTP::Head)
    {
        curl_code = doHead();
    }
    else
    {
        curl_code = CURLE_UNSUPPORTED_PROTOCOL;
    }

    std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->takeReply(m_id);
    if (!isCanceled() && reply.get())
    {
        bool success = (curl_code == CURLE_OK);
        reply->replyResult(success);
        reply.reset();
    }

    setRunning(false);
    return curl_code;
}

void CURLWrapper::cancel()
{
    setCanceled(true);
}

int CURLWrapper::doPostGet()
{
    CURL* curl = curl_easy_init();
    if (curl)
    {
        curl_slist*	http_headers = nullptr;
        CURLcode curl_code = publicSetoptMethod(curl, http_headers);

        std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);
        if (reply.get())
        {
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &reply->m_receive_header);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply->m_receive_content);
        }
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
        if (m_type == HTTP::Post)
        {
            curl_code = curl_easy_setopt(curl, CURLOPT_POST, 1);
            if (curl_code == CURLE_OK)
            {
                curl_code = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, m_post_data.c_str());
                curl_code = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, m_post_data.size());
            }
        }

        if (curl_code != CURLE_OK)
        {
            setFailed(true);
            if (nullptr != http_headers)
            {
                curl_slist_free_all(http_headers);
            }
            curl_easy_cleanup(curl);
            return curl_code;
        }

        curl_code = curl_easy_perform(curl);
        if (curl_code == CURLE_OPERATION_TIMEDOUT)
        {
            int retry_count = m_retry_times;
            while (!isCanceled() && retry_count > 0)
            {
                curl_code = curl_easy_perform(curl);
                if (curl_code != CURLE_OPERATION_TIMEDOUT)
                {
                    break;
                }
                retry_count--;
            }
        }

        if (reply.get())
        {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply->m_http_code);
            if (curl_code != CURLE_OK || reply->m_http_code != 200)
            {
                const char* err_string = curl_easy_strerror(curl_code);
                reply->m_error_string = err_string;
            }
        }

        if (nullptr != http_headers)
        {
            curl_slist_free_all(http_headers);
        }
        curl_easy_cleanup(curl);
        return curl_code;
    }

    return CURLE_FAILED_INIT;
}

int CURLWrapper::doDownload()
{
    m_total_size = getDownloadFileSize();
    _INT64 size = m_total_size;
    LOG_DEBUG("[HttpRequest] file size: %lld\n", size);

    if (m_total_size < 0)
    {
        m_multi_download = false;
    }

    std::string file_name = m_file_path;
    std::string out_file_name = file_name + ".dl";
    DeleteFileA(out_file_name.c_str());

    FILE* fp = nullptr;
    errno_t err = fopen_s(&fp, out_file_name.c_str(), "wb+");
    if (err != 0 || nullptr == fp)
    {
        std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);
        if (reply.get())
        {
            reply->m_error_string = "Open local file error!";
        }
        return CURLE_READ_ERROR;
    }

    int ret_code = CURLE_OK;
    if (m_multi_download)
    {
        int thread_count = splitDownloadCount(m_total_size);
        m_thread_count = (thread_count > m_thread_count ? m_thread_count : thread_count);
        if (m_thread_count <= 1)
        {
            m_multi_download = false;
        }
    }
    LOG_DEBUG("[HttpRequest]multi-download: %d; thread-count: %d\n", (int)m_multi_download, m_thread_count);

    //文件大小有分开下载的必要并且服务器支持多线程下载时，启用多线程下载
    if (m_multi_download && m_thread_count > 1)
    {
        long gap = static_cast<long>(m_total_size) / m_thread_count;
        std::vector<HANDLE> threads;

        std::vector<std::unique_ptr<DownloadChunk>> chunks;
        chunks.reserve(m_thread_count);

        for (int i = 0; i < m_thread_count; i++)
        {
#if !defined(_MSC_VER) || _MSC_VER >= 1700
            std::unique_ptr<DownloadChunk> chunk = std::make_unique<DownloadChunk>();
#else
            std::unique_ptr<DownloadChunk> chunk(new DownloadChunk);
#endif
            chunk->_fp = fp;
            chunk->_request = this;

            if (i < m_thread_count - 1)
            {
                chunk->_startidx = i * gap;
                chunk->_endidx = chunk->_startidx + gap;
            }
            else
            {
                chunk->_startidx = i * gap;
                chunk->_endidx = m_total_size - 1;
            }
            LOG_DEBUG("[HttpRequest]Part %d, Range: %d - %d\n", i, chunk->_startidx, chunk->_endidx);

            UINT thread_id;
            HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, &CURLWrapper::downloadProc, chunk.get(), 0, &thread_id);
            chunks.emplace_back(std::move(chunk));

            threads.emplace_back(hThread);
        }

        WaitForMultipleObjects(threads.size(), &threads[0], TRUE, INFINITE);
        for (int i = 0; i < threads.size(); ++i)
        {
            HANDLE handle = threads[i];
            CloseHandle(handle);
        }

        chunks.clear();
    }
    else
    {
#if !defined(_MSC_VER) || _MSC_VER >= 1700
        std::unique_ptr<DownloadChunk> chunk = std::make_unique<DownloadChunk>();
#else
        std::unique_ptr<DownloadChunk> chunk(new DownloadChunk);
#endif
        chunk->_fp = fp;
        chunk->_request = this;
        chunk->_startidx = 0;
        chunk->_endidx = 0;
        download(chunk.get());
    }

    fclose(fp);

    if (!isFailed())
    {
        ret_code = CURLE_OK;
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
    CURL* curl = curl_easy_init();
    if (curl)
    {
        std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);

        curl_slist*	http_headers = nullptr;
        CURLcode curl_code = publicSetoptMethod(curl, http_headers);
        if (curl_code != CURLE_OK)
        {
            LOG_DEBUG("[HttpRequest] publicSetoptMethod error! [%ul]\n", GetLastError());
            if (reply.get())
            {
                const char* err_string = curl_easy_strerror(curl_code);
                reply->m_error_string = err_string;
            }
            setFailed(true);
            if (nullptr != http_headers)
            {
                curl_slist_free_all(http_headers);
            }
            curl_easy_cleanup(curl);
            return curl_code;
        }

        FILE * file;
        errno_t err = fopen_s(&file, m_strUploadFile.c_str(), "rb");
        if (err != 0 || nullptr == file)
        {
            LOG_DEBUG("[HttpRequest] fopen_s error! [%ul]\n", GetLastError());
            curl_code = CURLE_READ_ERROR;
            setFailed(true);
            curl_slist_free_all(http_headers);
            curl_easy_cleanup(curl);
            return curl_code;
        }

        std::unique_ptr<UploadChannel> uc;
        if (reply.get())
        {
            /* get the file size of the local file */
            struct _stat64 file_info = { 0 };
            _stat64(m_strUploadFile.c_str(), &file_info);

#if !defined(_MSC_VER) || _MSC_VER >= 1700
            uc = std::make_unique<UploadChannel>();
#else
            uc.reset(new UploadChannel);
#endif
            uc->_fp = file;
            uc->_request = this;

            curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_callback);
            curl_easy_setopt(curl, CURLOPT_READDATA, uc.get());
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                (curl_off_t)file_info.st_size);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply->m_receive_content);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_upload_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        }

        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        /* HTTP PUT please */
        curl_easy_setopt(curl, CURLOPT_PUT, 1L);

        curl_code = curl_easy_perform(curl);
        if (curl_code == CURLE_OPERATION_TIMEDOUT)
        {
            int retry_count = m_retry_times;
            while (!isCanceled() && retry_count > 0)
            {
                curl_code = curl_easy_perform(curl);
                if (curl_code != CURLE_OPERATION_TIMEDOUT)
                {
                    break;
                }
                retry_count--;
            }
        }

        if (reply.get())
        {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply->m_http_code);
            if (curl_code != CURLE_OK || reply->m_http_code != 200)
            {
                const char* err_string = curl_easy_strerror(curl_code);
                reply->m_error_string = err_string;
            }
        }

        fclose(file);
        if (http_headers)
        {
            curl_slist_free_all(http_headers);
        }
        curl_easy_cleanup(curl);
        return curl_code;
    }

    return CURLE_FAILED_INIT;
}

int CURLWrapper::doFormPostUpload()
{
    CURL* curl = curl_easy_init();
    if (curl)
    {
        std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);

        curl_slist*	http_headers = nullptr;
        CURLcode curl_code = publicSetoptMethod(curl, http_headers);
        if (curl_code != CURLE_OK)
        {
            if (reply.get())
            {
                const char* err_string = curl_easy_strerror(curl_code);
                reply->m_error_string = err_string;
            }
            setFailed(true);
            if (nullptr != http_headers)
            {
                curl_slist_free_all(http_headers);
            }
            curl_easy_cleanup(curl);
            return curl_code;
        }

        if (reply.get())
        {
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply->m_receive_content);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_upload_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        }

        struct curl_httppost* httppost = NULL;
        struct curl_httppost* lastpost = NULL;
        /*
        Fill in the file upload field. This makes libcurl load data from
        the given file name when curl_easy_perform() is called.
        */
        curl_formadd(&httppost,
            &lastpost,
            CURLFORM_COPYNAME, "sendfile",
            CURLFORM_FILE, m_strUploadFile.c_str(),
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

        curl_easy_setopt(curl, CURLOPT_HTTPPOST, httppost);

        curl_code = curl_easy_perform(curl);
        if (curl_code == CURLE_OPERATION_TIMEDOUT)
        {
            int retry_count = m_retry_times;
            while (!isCanceled() && retry_count > 0)
            {
                curl_code = curl_easy_perform(curl);
                if (curl_code != CURLE_OPERATION_TIMEDOUT)
                {
                    break;
                }
                retry_count--;
            }
        }

        if (reply.get())
        {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply->m_http_code);
            if (curl_code != CURLE_OK || reply->m_http_code != 200)
            {
                const char* err_string = curl_easy_strerror(curl_code);
                reply->m_error_string = err_string;
            }
        }

        if (httppost)
        {
            curl_formfree(httppost);
        }
        if (http_headers)
        {
            curl_slist_free_all(http_headers);
        }
        curl_easy_cleanup(curl);
        return curl_code;
    }

    return CURLE_FAILED_INIT;
}

int CURLWrapper::doHead()
{
    CURLcode curl_code = CURLE_FAILED_INIT;
    CURL* curl = curl_easy_init();
    if (curl)
    {
        std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);

        curl_slist*	http_headers = nullptr;
        curl_code = publicSetoptMethod(curl, http_headers);
        if (curl_code != CURLE_OK)
        {
            if (reply.get())
            {
                const char* err_string = curl_easy_strerror(curl_code);
                reply->m_error_string = err_string;
            }
            setFailed(true);
            if (nullptr != http_headers)
            {
                curl_slist_free_all(http_headers);
            }
            curl_easy_cleanup(curl);
            return curl_code;
        }

        curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        if (reply.get())
        {
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &reply->m_receive_header);
        }

        curl_code = curl_easy_perform(curl);
        if (curl_code == CURLE_OPERATION_TIMEDOUT)
        {
            int retry_count = m_retry_times;
            while (!isCanceled() && retry_count > 0)
            {
                curl_code = curl_easy_perform(curl);
                if (curl_code != CURLE_OPERATION_TIMEDOUT)
                {
                    break;
                }
                retry_count--;
            }
        }

        if (reply.get())
        {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply->m_http_code);
            if (curl_code != CURLE_OK || reply->m_http_code != 200)
            {
                const char* err_string = curl_easy_strerror(curl_code);
                reply->m_error_string = err_string;
            }
        }

        if (http_headers)
        {
            curl_slist_free_all(http_headers);
        }
        curl_easy_cleanup(curl);
        return curl_code;
    }
    return CURLE_FAILED_INIT;
}

int CURLWrapper::download(DownloadChunk* chunk)
{
    CURLcode curl_code = CURLE_FAILED_INIT;
    CURL* curl = curl_easy_init();
    if (curl)
    {
        std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);

        curl_slist*	http_headers = nullptr;
        curl_code = publicSetoptMethod(curl, http_headers);
        if (curl_code != CURLE_OK)
        {
            if (reply.get())
            {
                const char* err_string = curl_easy_strerror(curl_code);
                reply->m_error_string = err_string;
            }
            setFailed(true);
            if (nullptr != http_headers)
            {
                curl_slist_free_all(http_headers);
            }
            curl_easy_cleanup(curl);
            return curl_code;
        }

        curl_easy_setopt(curl, CURLOPT_POST, 0L);
        if (reply.get())
        {
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, chunk);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_download_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, chunk);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 5L);
        }

        //const char* user_agent = ("Mozilla/5.0 (Windows NT 5.1; rv:5.0) Gecko/20100101 Firefox/5.0");
        //curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, user_agent);

        if (chunk && chunk->_endidx != 0)
        {
            std::string range;
            std::ostringstream ostr;
            if (chunk->_endidx > 0)
            {
                ostr << chunk->_startidx << "-" << chunk->_endidx;
            }
            else
            {
                ostr << chunk->_startidx << "-";
            }

            range = ostr.str();
            curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
        }

        curl_code = curl_easy_perform(curl);
        if (curl_code == CURLE_OPERATION_TIMEDOUT)
        {
            int retry_count = m_retry_times;
            while (!isFailed() && retry_count > 0)
            {
                curl_code = curl_easy_perform(curl);
                if (curl_code != CURLE_OPERATION_TIMEDOUT)
                {
                    break;
                }
                retry_count--;
            }
        }

        bool chunk_download_failed = true;
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (curl_code == CURLE_OK && (http_code == 200 || http_code == 206))
        {
            chunk_download_failed = false;
        }

        if (chunk_download_failed)
        {
            setFailed(true);
            if (reply.get())
            {
                if (curl_code != CURLE_OK)
                {
                    const char* err_string = curl_easy_strerror(curl_code);
                    reply->m_error_string = err_string;
                }
                else
                {
                    char ch[32];
                    sprintf_s(ch, "http code[%d]", http_code);
                    reply->m_error_string = ch;
                }
            }
            if (reply.get())
            {
                reply->m_http_code = http_code;
            }
        }
        else
        {
            //沒赋值过错误码，就赋值个200，保证不会把之前错误的冲掉
            if (reply->m_http_code != 0)
            {
                reply->m_http_code = 200;
            }
        }

        curl_slist_free_all(http_headers);
        curl_easy_cleanup(curl);
    }

    return curl_code;
}

_INT64 CURLWrapper::getDownloadFileSize()
{
    if (m_url.empty())
    {
        return -1;
    }
    else
    {
        _INT64 file_size = -1;
        CURL* curl_handle = curl_easy_init();
        if (curl_handle)
        {
            std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);

            curl_slist*	http_headers = nullptr;
            CURLcode curl_code = publicSetoptMethod(curl_handle, http_headers);
            if (curl_code != CURLE_OK)
            {
                if (reply.get())
                {
                    const char* err_string = curl_easy_strerror(curl_code);
                    reply->m_error_string = err_string;
                }
                setFailed(true);
                if (nullptr != http_headers)
                {
                    curl_slist_free_all(http_headers);
                }
                curl_easy_cleanup(curl_handle);
                return -1;
            }

            curl_easy_setopt(curl_handle, CURLOPT_HEADER, 1L);
            curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1L);
            //curl_easy_setopt(curl_handle, CURLOPT_RANGE, "0-1");
            if (reply.get())
            {
                curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_callback);
                curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &reply->m_receive_header);
            }

            long http_code = 0;
            curl_code = curl_easy_perform(curl_handle);
            curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
            if (curl_code == CURLE_OK && http_code == 200)
            {
                double size = -1.0;
                curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);
                file_size = size;

                if (reply.get())
                {
                    //匹配"Accept-Ranges: bytes" 则证明支持多线程下载
                    std::regex pattern("Accept-Ranges: bytes", std::regex::icase);
                    m_multi_download = std::regex_search(reply->m_receive_header, pattern);
                }
            }
            else if (reply.get())
            {
                const char* err_string = curl_easy_strerror(curl_code);
                reply->m_error_string = err_string;
            }
            if (reply.get())
            {
                reply->m_http_code = http_code;
            }

            if (nullptr != http_headers)
            {
                curl_slist_free_all(http_headers);
            }
            curl_easy_cleanup(curl_handle);
        }

        return file_size;
    }
}

int CURLWrapper::splitDownloadCount(_INT64 total_size)
{
    const _INT64 size_2MB = 2 * 1024 * 1024;
    const _INT64 size_10MB = 10 * 1024 * 1024;
    const _INT64 size_50MB = 50 * 1024 * 1024;

    int max_count = DEFAULT_DOWNLOAD_THREAD_COUNT * 2;

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
        return DEFAULT_DOWNLOAD_THREAD_COUNT;
    }
    else
    {
        int count = static_cast<int>(total_size / size_10MB);
        return count > max_count ? max_count : count;
    }

    return 1;
}

UINT WINAPI CURLWrapper::downloadProc(LPVOID param)
{
    DownloadChunk* thread_chunk = reinterpret_cast<DownloadChunk*>(param);
    if (thread_chunk)
    {
        CURLWrapper* pRequest = thread_chunk->_request;
        if (pRequest)
        {
            return pRequest->download(thread_chunk);
        }
    }
    return -1;
}

void CURLWrapper::reset()
{
    setRunning(false);
    setCanceled(false);
    setFailed(false);
    m_multi_download = false;
    m_thread_count = 1;
    m_total_size = 0.0;
    m_current_size = 0.0;
}

//////////////////////////////////////////////////////////////////////////
HttpRequest::HttpRequest()
    : m_handler(new CURLWrapper)
{
    TRACE_CLASS_CONSTRUCTOR(HttpRequest);
    HttpManager::globalInstance();
}

HttpRequest::~HttpRequest()
{
    TRACE_CLASS_DESTRUCTOR(HttpRequest);
    m_handler = nullptr;
}

int HttpRequest::setRetryTimes(int retry_times)
{
    if (m_handler.get())
    {
        return m_handler->setRetryTimes(retry_times);
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setTimeout(long time_out)
{
    if (m_handler.get())
    {
        return m_handler->setTimeout(time_out);
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setUrl(const std::string& url)
{
    if (m_handler.get())
    {
        return m_handler->setUrl(url);
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setFollowLocation(bool follow_location)
{
    if (m_handler.get())
    {
        return m_handler->setFollowLocation(follow_location);
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setPostData(const std::string& message)
{
    return setPostData(message.c_str(), message.size());
}

int HttpRequest::setPostData(const char* data, unsigned int size)
{
    if (m_handler.get())
    {
        return m_handler->setPostData(data, size);
    }
    return REQUEST_INIT_ERROR;
}

int HttpRequest::setHeader(const std::map<std::string, std::string>& headers)
{
    if (m_handler.get())
    {
        for (auto it = headers.begin(); it != headers.end(); ++it)
        {
            std::string header = it->first;
            header += ": ";
            header += it->second;
            m_handler->setHeader(header);
        }
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setHeader(const std::string& header)
{
    if (m_handler.get())
    {
        return m_handler->setHeader(header);
    }
    return REQUEST_INIT_ERROR;
}

int HttpRequest::setProxy(const std::string& proxy, long proxy_port)
{
    if (m_handler.get())
    {
        return m_handler->setProxy(proxy, proxy_port);
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setDownloadFile(const std::string& file_path, int thread_count /* = 5 */)
{
    if (m_handler.get())
    {
        m_handler->setDownloadFile(file_path);
        m_handler->setDownloadThreadCount(thread_count);
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setUploadFile(const std::string& file_path)
{
    if (m_handler.get())
    {
        m_handler->setUploadFile(file_path);
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::setUploadFile(const std::string& file_path, const std::string& target_name, const std::string& target_path)
{
    if (m_handler.get())
    {
        m_handler->setUploadFile(file_path, target_name, target_path);
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

int HttpRequest::registerResultCallback(HTTP::ResultCallback rc)
{
    if (m_handler.get())
    {
        m_handler->registerResultCallback(rc);
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

int	HttpRequest::registerProgressCallback(HTTP::ProgressCallback pc)
{
    if (m_handler.get())
    {
        m_handler->registerProgressCallback(pc);
        return REQUEST_OK;
    }

    return REQUEST_INIT_ERROR;
}

std::shared_ptr<HttpReply> HttpRequest::perform(HTTP::RequestType type, HTTP::IOMode mode)
{
    std::shared_ptr<HttpReply> reply = nullptr;
    if (m_handler.get())
    {
        if (m_handler->isRunning())
            m_handler->cancel();

        int nId = m_handler->requestId();
        m_handler->setRequestType(type);
        m_handler->setIOMode(mode);

        reply = std::make_shared<HttpReply>(nId);
        HttpManager::globalInstance()->addReply(reply);

        if (mode == HTTP::Sync)
        {
            m_handler->perform();
        }
        else if (mode == HTTP::Async)
        {
#if defined(_MSC_VER) && _MSC_VER < 1700
            std::unique_ptr<HttpTask> task(new HttpTask(true));
#else
            std::unique_ptr<HttpTask> task = std::make_unique<HttpTask>(true);
#endif
            task->attach(m_handler);
            HttpManager::addTask(std::move(task));
        }
    }

    return reply;
}

bool HttpRequest::cancel(int requestId)
{
    return HttpManager::abortTask(requestId);
}

bool HttpRequest::cancelAll()
{
    return HttpManager::abortAllTask();
}

void HttpRequest::globalInit()
{
    return HttpManager::globalInit();
}

void HttpRequest::globalCleanup()
{
    return HttpManager::globalCleanup();
}