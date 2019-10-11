#ifndef __HTTP_MANAGER_H
#define __HTTP_MANAGER_H
#pragma once

#include <map>
#include <curl/curl.h>
#include "Lock.h"


class TaskBase;
class HttpReply;
// class HttpManager - Http管理类
class HttpManager
{
public:
    virtual ~HttpManager();
#if _MSC_VER >= 1700
    HttpManager(const HttpManager &) = delete;
    HttpManager &operator=(const HttpManager &) = delete;
#endif

    static HttpManager* globalInstance();
    static void globalInit();
    static void globalCleanup();

    void addReply(std::shared_ptr<HttpReply> reply);
    void removeReply(int);
    std::shared_ptr<HttpReply> takeReply(int);
    std::shared_ptr<HttpReply> getReply(int);

    DWORD mainThreadId() { return m_mainThreadId; }

private:
    HttpManager();
#if _MSC_VER < 1700
    HttpManager(const HttpManager &);
    HttpManager &operator=(const HttpManager &);
#endif

private:
    static void set_share_handle(CURL* curl_handle);
    static bool addTask(std::unique_ptr<TaskBase> t);
    static bool abortTask(int taskId);
    static bool abortAllTask();
    void clearReply();

private:
    static CURLSH* s_share_handle_;
    std::map<int, std::shared_ptr<HttpReply>> m_map_replys;
    mutable VCUtil::CSLock m_lock;
    DWORD m_mainThreadId;

    friend class CURLWrapper;
    friend class HttpRequest;
};

#endif //__HTTP_MANAGER_H

