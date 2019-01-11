HttpRequest moudle
======================================================
@version: 1.0.0.3  
@Author: Vilas Wang  
@Contact: QQ451930733 | 451930733@qq.com  


** HttpRequest is free library licensed under the term of LGPL v3.0. **



## Detailed Description


HttpRequest库是对libcurl的封装，结合C++线程池，实现http多线程异步/同步请求。
- 多任务并发执行
- 所有任务异步调用
- 所有方法线程安全

本模块使用的是vs2015编译的libcurl静态库，版本不同请自行编译。  
sample项目是Qt写的界面。


## How to use

>异步下载：
> 

```cpp
#include "HttpRequest.h"

const std::string strUrl = "...";
const std::string strFilePath = "...";

HttpRequest request(HttpRequest::Download);
request.setRequestUrl();
request.setDownloadFile(strFilePath);
request.setFollowLocation(true);
request.setResultCallback(std::bind(&CurlTool::onRequestResultCallback, this, 
	std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
request.setProgressCallback(std::bind(&CurlTool::onProgressCallback, this, 
	std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

int nReqId = request.perform(HttpRequest::Async);
```


>异步上传
>

```cpp
#include "HttpRequest.h"

const std::string strUrl = "...";
const std::string strUploadFilePath = "...";
const std::string strTargetName = "...";
const std::string strSavePath = "...";

HttpRequest request(HttpRequest::Upload);
request.setRequestUrl(strUrl);
request.setUploadFile(strUploadFilePath, strTargetName, strSavePath);
request.setFollowLocation(true);
request.setResultCallback(std::bind(&CurlTool::onRequestResultCallback, this, 
	std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
request.setProgressCallback(std::bind(&CurlTool::onProgressCallback, this, 
	std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

int nReqId = request.perform(HttpRequest::Async);
```

