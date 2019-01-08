HttpRequest moudle
======================================================
@version: 1.0.1  
@Author: Vilas Wang  
@Contact: QQ451930733  
@Email: vilas900420@gmail.com / 451930733@qq.com

Copyright © 2018 Vilas Wang. All rights reserved.

** HttpRequest is free library licensed under the term of LGPL v3.0. **



## Detailed Description


HttpRequest库是对libcurl的封装，结合C++线程池，实现http多线程异步/同步请求。
- 多任务并发执行
- 所有任务异步调用
- 所有方法线程安全



## How to use

>下载：
> 

```cpp
#include "HttpRequest.h"

const std::string strUrl = "...";
const std::string strFilePath = "...";

HttpRequest req(HttpRequest::DWONLOAD);
req.setRequestUrl();
req.setDownloadFile(strFilePath);
req.setFollowLocation(true);
req.setResultCallback(std::bind(&CurlTool::onRequestResultCallback, this, 
	std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
req.setProgressCallback(std::bind(&CurlTool::onProgressCallback, this, 
	std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

int nId = req.perform(HttpRequest::ASYNC);
```


>上传
>

```cpp
#include "HttpRequest.h"

const std::string strUrl = "...";
const std::string strUploadFilePath = "...";
const std::string strTargetName = "...";
const std::string strSavePath = "...";

HttpRequest req(HttpRequest::UPLOAD);
req.setRequestUrl(strUrl);
req.setUploadFile(strUploadFilePath, strTargetName, strSavePath);
req.setFollowLocation(true);
req.setResultCallback(std::bind(&CurlTool::onRequestResultCallback, this, 
	std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
req.setProgressCallback(std::bind(&CurlTool::onProgressCallback, this, 
	std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

int nId = req.perform(HttpRequest::ASYNC);
```

