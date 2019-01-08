#ifndef __HTTP_REQUEST_H
#define __HTTP_REQUEST_H

#include <string>
#include <map>
#include <vector>
#include <functional>
#include "HttpRequestManager.h"
#include "HttpRequest_global.h"

#ifndef INT64
#define INT64 signed long long int
#endif

//{int id, bool success, const std::string& data, const std::string& error_string}
typedef std::function<void(int, bool, const std::string&, const std::string&)> ResultCallback;
//{int id, bool is_download, INT64 total_size, INT64 downloaded_size}
typedef std::function<void(int, bool, INT64, INT64)> ProgressCallback;


class CURLWrapper;
class HTTP_REQUEST_EXPORT HttpRequest
{
public:
    enum RequestType
    {
        Post,
        Get,
        Download,
        Upload,
    };

    enum CallType
    {
        Sync,
        Async,
    };

    enum RequestResult
    {
        REQUEST_OK,
        REQUEST_INVALID_OPT,
        REQUEST_PERFORM_ERROR,
        REQUEST_OPENFILE_ERROR,
        REQUEST_INIT_ERROR,
    };

public:
    HttpRequest(RequestType type);
    ~HttpRequest();

    int setRetryTimes(int retry_times);
    int setRequestTimeout(long time_out = 0);
    int setRequestUrl(const std::string& url);
    int setRequestProxy(const std::string& proxy, long proxy_port);

    // Description: set http redirect follow location
    int setFollowLocation(bool follow_location);

    // Description: set http request header, for example : Range:bytes=554554-
    int setRequestHeader(const std::map<std::string, std::string>& headers);
    int setRequestHeader(const std::string& header);

    // Description: 若调用该方法就是post方式请求，否则是curl默认get
    int setPostData(const std::string& data);
    int setPostData(const char* data, unsigned int size);

    int setDownloadFile(const std::string& file_path, int thread_count = 5);
    int setUploadFile(const std::string& file_path, const std::string& target_name, const std::string& target_path);

	//开始请求，并返回requestId; 成功：非0
    int perform(CallType type);
	//取消请求
	static bool cancel(int requestId);
	//取消所有请求
    static bool cancelAll();
	//取消所有请求，清理所有curl资源，让线程池所有线程取消任务并退出
	static void globalCleanup();

    //异步回調api
	//最好不是类的非静态方法。以免回调之前类析构了
    int setResultCallback(ResultCallback rc);
    int	setProgressCallback(ProgressCallback pc);

    //以下几个同步有效
    bool getHttpCode(long& http_code);
    bool getReceiveHeader(std::string& header);
    bool getReceiveContent(std::string& receive);
    bool getErrorString(std::string& error_string);

private:
    std::shared_ptr<CURLWrapper> m_request_helper;
};


class RequestImpl
{
public:
	RequestImpl() {}
	virtual ~RequestImpl() {}
public:
	virtual int	perform() = 0;
	virtual void cancel() = 0;
	virtual int	requestId() = 0;
};
#endif  /*__HTTP_REQUEST_H*/
