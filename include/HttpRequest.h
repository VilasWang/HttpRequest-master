#ifndef __HTTP_REQUEST_H
#define __HTTP_REQUEST_H

#include <string>
#include <map>
#include <vector>

#include "HttpManager.h"
#include "HttpReply.h"
#include "httprequestdef.h"
#include "HttpRequest_global.h"


class CURLWrapper;
// class HttpRequest - Http请求类（libcurl）
class HTTP_REQUEST_EXPORT HttpRequest
{
public:
	enum IOMode
	{
		Sync,
		Async,
	};

	enum RequestResult
	{
		REQUEST_OK,
		REQUEST_INVALID_OPT,
		REQUEST_PERFORM_ERROR,
		REQUEST_INIT_ERROR,
	};

public:
	explicit HttpRequest();
	~HttpRequest();

	// 初始化libcurl资源，初始化线程池(需在主线程中)
	static void globalInit();

	// 取消所有请求，清理libcurl资源，让线程池所有线程取消任务并退出(需在主线程中)
	static void globalCleanup();

public:
	// 开始请求，并返回HttpReply. 
	// 1：同步请求可以直接调用HttpReply的接口获取结果
	// 2：异步请求可以设置异步回调接口，请求结束时自动回调获取结果
	// 注意点：	1.异步请求的时候， 需要把返回的std::shared_ptr<HttpReply>根据id保存起来；
	//				等收到结束回调的时候，再把std::shared_ptr<HttpReply>置空，不然会收不到结束回调。
	//			2.异步请求的回调接口都是在curl执行的工作线程调用，所以根据不同情况，自己再做一些处理。
	//				比如回调接口中加锁访问资源或者把回调结果再post的你自己的线程中处理。(比较好的是后者)
	std::shared_ptr<HttpReply> perform(HttpRequestType rtype, IOMode mode = Async);
	// 取消请求
	static bool cancel(int requestId);
	// 取消所有请求
	static bool cancelAll();

	// 异步回调api
	// 最好不要用类的非静态成员函数。以免回调返回时类已析构
	int setResultCallback(ResultCallback rc);
	int	setProgressCallback(ProgressCallback pc);

	int setRetryTimes(int retry_times);
	int setTimeout(long time_out = 0); // 请求超时（second）
	int setUrl(const std::string& url);
	int setProxy(const std::string& proxy, long proxy_port);
	// set http redirect follow location
	int setFollowLocation(bool follow_location);
	// set http request header, for example : Range:bytes=554554-
	int setHeader(const std::map<std::string, std::string>& headers);
	int setHeader(const std::string& header);

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
