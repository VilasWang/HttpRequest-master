#ifndef __HTTP_REQUEST_H
#define __HTTP_REQUEST_H

#include <string>
#include <map>
#include <vector>

#include "HttpManager.h"
#include "HttpReply.h"
#include "HttpRequest_global.h"


class CURLWrapper;
// class HttpRequest - Http请求类（libcurl）
class HTTP_REQUEST_EXPORT HttpRequest
{
public:
	enum RequestType
	{
		Post,
		Get,
		Download,
		Upload,		//HTTP put 方式上次文件 (文件名和路径需在url中指定)
		Upload2,	//HTTP Multipart formpost 方式上次文件(大小受限于服务器post数据的大小)
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
	explicit HttpRequest();
	~HttpRequest();

	// 开始请求，并返回HttpReply. 
	// 1：同步请求可以直接调用HttpReply的接口获取结果
	// 2：异步请求可以设置异步回调接口，请求结束时自动回调获取结果
	std::shared_ptr<HttpReply> perform(RequestType rtype, CallType ctype = Async);
	// 取消请求
	static bool cancel(int requestId);
	// 取消所有请求
	static bool cancelAll();
	// 取消所有请求，清理所有curl资源，让线程池所有线程取消任务并退出
	static void globalCleanup();

	// 异步回调api
	// 最好不要用类的非静态方法。以免回调时类已析构
	int setResultCallback(ResultCallback rc);
	int	setProgressCallback(ProgressCallback pc);

	int setRetryTimes(int retry_times);
	int setRequestTimeout(long time_out = 0); // 请求超时（second）
	int setRequestUrl(const std::string& url);
	int setRequestProxy(const std::string& proxy, long proxy_port);
	// set http redirect follow location
	int setFollowLocation(bool follow_location);
	// set http request header, for example : Range:bytes=554554-
	int setRequestHeader(const std::map<std::string, std::string>& headers);
	int setRequestHeader(const std::string& header);

	// 若调用该方法就是post方式请求，否则是curl默认get
	int setPostData(const std::string& data);
	int setPostData(const char* data, unsigned int size);

	int setDownloadFile(const std::string& file_path, int thread_count = 5);
	//HTTP put 方式上次文件 (文件名和路径在url中需要指定)
	int setUploadFile(const std::string& file_path);
	//HTTP Multipart formpost 方式上次文件
	int setUploadFile(const std::string& file_path, const std::string& target_name, const std::string& target_path);

private:
	std::shared_ptr<CURLWrapper> m_helper;
};

#endif  /*__HTTP_REQUEST_H*/
