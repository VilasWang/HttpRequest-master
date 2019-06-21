#pragma once

#include <functional>
#include <memory>

#ifndef INT64
#define INT64 long long int
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
}

#if _MSC_VER >= 1700
// int id, bool success, const std::string& data, const std::string& error_string
using ResultCallback = std::function<void(int, bool, const std::string&, const std::string&)>;

// int id, bool is_download, INT64 total_size, INT64 current_size
using ProgressCallback = std::function<void(int, bool, INT64, INT64)>;

#else
// int id, bool success, const std::string& data, const std::string& error_string
typedef std::function<void(int, bool, const std::string&, const std::string&)> ResultCallback;

// int id, bool is_download, INT64 total_size, INT64 current_size
typedef std::function<void(int, bool, INT64, INT64)> ProgressCallback;

#endif


class IRequest
{
public:
    IRequest() {}
    virtual ~IRequest() {}
public:
    virtual int	perform() = 0;
    virtual void cancel() = 0;
    virtual int	requestId() = 0;
    virtual bool isRunning() const = 0;
    virtual bool isCanceled() const = 0;
    virtual bool isFailed() const = 0;
};


