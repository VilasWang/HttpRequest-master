﻿#include "HttpManager.h"
#include "HttpRequest.h"
#include <iostream>
#include "ClassMemoryTracer.h"
#include "Log.h"
#include "ThreadPool/ThreadPool.h"
#include "HttpReply.h"

using namespace VCUtil;

namespace {
    static SRWLock s_lock;

    void curlLock(CURL *handle, curl_lock_data data, curl_lock_access
        access, void *useptr)
    {
        (void)handle;
        (void)useptr;
        if (data == CURL_LOCK_DATA_DNS)
        {
            if (access == CURL_LOCK_ACCESS_SHARED)
            {
                s_lock.lock(true);
            }
            else if (access == CURL_LOCK_ACCESS_SINGLE)
            {
                s_lock.lock(false);
            }
        }
    }

    void curlUnlock(CURL *handle, curl_lock_data data, void *useptr)
    {
        (void)handle;
        (void)useptr;
        if (data == CURL_LOCK_DATA_DNS)
        {
            s_lock.unlock();
        }
    }
}

CURLSH* HttpManager::s_share_handle_ = nullptr;
HttpManager::HttpManager()
{
    TRACE_CLASS_CONSTRUCTOR(HttpManager);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    //s_share_handle_ = curl_share_init();
    //curl_share_setopt(s_share_handle_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    //curl_share_setopt(s_share_handle_, CURLSHOPT_LOCKFUNC, curlLock);
    //curl_share_setopt(s_share_handle_, CURLSHOPT_UNLOCKFUNC, curlUnlock);

    m_mainThreadId = GetCurrentThreadId();
    LOG_DEBUG("Main Thread: %d\n", m_mainThreadId);
    ThreadPool::globalInstance()->init(8);
}

HttpManager::~HttpManager()
{
    //LOG_DEBUG("%s (B)\n", __FUNCTION__);
    TRACE_CLASS_DESTRUCTOR(HttpManager);

    globalCleanup();
    //curl_share_cleanup(s_share_handle_);
    curl_global_cleanup();

    //LOG_DEBUG("%s (E)\n", __FUNCTION__);
}

HttpManager* HttpManager::globalInstance()
{
    static HttpManager _instance;
    return &_instance;
}

void HttpManager::globalInit()
{
    HttpManager::globalInstance();
}

void HttpManager::globalCleanup()
{
    HttpManager::globalInstance()->clearReply();
    ThreadPool::globalInstance()->waitForDone();
}

bool HttpManager::addTask(std::unique_ptr<TaskBase> t)
{
    if (!t.get())
    {
        return false;
    }
    return ThreadPool::globalInstance()->addTask(std::move(t));
}

bool HttpManager::abortTask(int task_id)
{
    return ThreadPool::globalInstance()->abortTask(task_id);
}

bool HttpManager::abortAllTask()
{
    HttpManager::globalInstance()->clearReply();
    return ThreadPool::globalInstance()->abortAllTask();
}

void HttpManager::clearReply()
{
    Locker<CSLock> locker(m_lock);
    m_map_replys.clear();
}

void HttpManager::set_share_handle(CURL* curl_handle)
{
    if (curl_handle && s_share_handle_)
    {
        curl_easy_setopt(curl_handle, CURLOPT_SHARE, s_share_handle_);
        curl_easy_setopt(curl_handle, CURLOPT_DNS_CACHE_TIMEOUT, 60 * 5);
    }
}

void HttpManager::addReply(std::shared_ptr<HttpReply> reply)
{
    Locker<CSLock> locker(m_lock);
    m_map_replys[reply->id()] = reply;
}

void HttpManager::removeReply(int id)
{
    Locker<CSLock> locker(m_lock);

    auto iter = m_map_replys.find(id);
    if (iter != m_map_replys.end())
    {
        m_map_replys.erase(iter);
    }
}

std::shared_ptr<HttpReply> HttpManager::takeReply(int id)
{
    std::shared_ptr<HttpReply> reply = nullptr;
    Locker<CSLock> locker(m_lock);

    auto iter = m_map_replys.find(id);
    if (iter != m_map_replys.end())
    {
        reply = iter->second;
        m_map_replys.erase(iter);
    }
    return reply;
}

std::shared_ptr<HttpReply> HttpManager::getReply(int id)
{
    std::shared_ptr<HttpReply> reply = nullptr;
    Locker<CSLock> locker(m_lock);

    auto iter = m_map_replys.find(id);
    if (iter != m_map_replys.end())
    {
        reply = iter->second;
    }
    return reply;
}