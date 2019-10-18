#pragma once

#include <functional>
#include <memory>

#ifndef _INT64
#define _INT64 signed long long //不支持的话改成__int64
#endif

namespace HTTP
{
    enum RequestType
    {
        Post = 0,
        Get,
        Download,
        Upload,		//HTTP put 方式上次文件 (文件名和路径需在url中指定)
        Upload2,	//HTTP Multipart formpost 方式上次文件(大小受限于服务器post数据的大小)
        Head,
        Unkonwn = -1
    };

    enum IOMode
    {
        Sync = 0,
        Async,
    };

#if defined(_MSC_VER) && _MSC_VER < 1700
    // int id, bool success, const std::string& contentt std::string& error
    typedef std::function<void(int, bool, const std::string&, const std::string&)> ResultCallback;

    // int id, bool is_download, _INT64 total_size, _INT64 current_size
    typedef std::function<void(int, bool, _INT64, _INT64)> ProgressCallback;
#else
    // int id, bool success, const std::string& contentt, const std::string& error
    using ResultCallback = std::function<void(int, bool, const std::string&, const std::string&)>;

    // int id, bool is_download, _INT64 total_size, _INT64 current_size
    using ProgressCallback = std::function<void(int, bool, _INT64, _INT64)>;
#endif


    //请求接口类
    class IRequest
    {
    public:
        virtual int	perform() = 0;
        virtual void cancel() = 0;
        virtual int	requestId() = 0;
        virtual bool isRunning() const = 0;
        virtual bool isCanceled() const = 0;
        virtual bool isFailed() const = 0;
    };
}
