#ifndef __HTTP_REQUEST_H
#define __HTTP_REQUEST_H

#include <string>
#include <map>
#include <vector>

#include "HttpManager.h"
#include "HttpReply.h"
#include "HttpRequest_global.h"


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
		Unkonwn = -1
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
	HttpRequest();
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
	std::shared_ptr<HttpReply> perform(RequestType rtype, CallType ctype = Async);
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

private:
	std::shared_ptr<CURLWrapper> m_helper;
};

#endif  /*__HTTP_REQUEST_H*/
