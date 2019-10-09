#ifndef __HTTP_REQUEST_H
#define __HTTP_REQUEST_H
#pragma once

#include <string>
#include <map>
#include <vector>

#include "HttpReply.h"
#include "HttpRequestDef.h"
#include "HttpRequest_global.h"


class CURLWrapper;
// class HttpRequest - Http请求类（libcurl）
class HTTP_REQUEST_EXPORT HttpRequest
{
public:
    enum RequestResult
    {
        REQUEST_OK,
        REQUEST_INVALID_OPT,
        REQUEST_INIT_ERROR,
        REQUEST_PERFORM_ERROR,
    };

public:
    HttpRequest();
    virtual ~HttpRequest();

    /// 以下两个接口必须在主线程中执行
    // 初始化libcurl资源，初始化线程池
    static void globalInit();
    // 取消所有请求，清理libcurl资源，让线程池所有线程取消任务并退出
    static void globalCleanup();

public:
    // 执行请求，并返回HttpReply. 
    // 1：同步请求可以直接调用HttpReply的接口获取结果
    // 2：异步请求可以设置异步回调接口（见setResultCallback），请求完成后会在主线程中异步回调（APC）
    std::shared_ptr<HttpReply> perform(HTTP::RequestType, HTTP::IOMode mode = HTTP::Async);
    // 取消某个请求
    static bool cancel(int requestId);
    // 取消所有请求
    static bool cancelAll();

    // 异步回调api
    // 推荐用lambda。如：
    //		auto onRequestResultCallback = [](int id, bool success, const std::string& content, const std::string& error){
    //			CurlTool::singleton()->replyResult(id, success, QString::fromStdString(content), QString::fromStdString(error));
    //		};
    //		HttpRequest req;
    //		req.setUrl("...");
    //		req.setResultCallback(onRequestResultCallback);
    //		req.perform(HTTP::Get, HTTP::Async);
    int setResultCallback(HTTP::ResultCallback rc);
    int	setProgressCallback(HTTP::ProgressCallback pc);

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
    std::shared_ptr<CURLWrapper> m_handler;
};

#endif  /*__HTTP_REQUEST_H*/
