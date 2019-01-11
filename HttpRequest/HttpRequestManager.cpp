#include "HttpRequestManager.h"
#include "HttpRequest.h"
#include "ClassMemoryTracer.h"

CURLSH* HttpRequestManager::s_share_handle_ = nullptr;

HttpRequestManager::HttpRequestManager()
    : m_callback(new HttpTaskCallBack)
    , m_lock(new TPLock)
{
	TRACE_CLASS_CONSTRUCTOR(HttpRequestManager);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    s_share_handle_ = curl_share_init();
    curl_share_setopt(s_share_handle_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);

    ThreadPool::Instance()->init();
    ThreadPool::Instance()->setCallBack(m_callback);
}

HttpRequestManager::~HttpRequestManager()
{
    char ch[64];
    sprintf_s(ch, "%s(B)\n", __FUNCTION__);
    OutputDebugStringA(ch);
	TRACE_CLASS_DESTRUCTOR(HttpRequestManager);

    ThreadPool::Instance()->waitForDone();
    clearTask();

    if (m_callback)
    {
        delete m_callback;
    }
    m_lock = nullptr;

    curl_share_cleanup(s_share_handle_);
    curl_global_cleanup();

    sprintf_s(ch, "%s(E)\n", __FUNCTION__);
    OutputDebugStringA(ch);

	TRACE_CLASS_PRINT();
}

HttpRequestManager* HttpRequestManager::Instance()
{
    static HttpRequestManager _instance;
    return &_instance;
}

bool HttpRequestManager::addTask(TaskBase* t, ThreadPool::Priority priority)
{
    if (!t->isAutoDelete() && t)
    {
        HttpRequestManager::Instance()->insertTask(t);
    }
    return ThreadPool::Instance()->addTask(t, priority);
}

bool HttpRequestManager::abortTask(int task_id)
{
    return ThreadPool::Instance()->abortTask(task_id);
}

bool HttpRequestManager::abortAllTask()
{
    return ThreadPool::Instance()->abortAllTask();
}

void HttpRequestManager::globalCleanup()
{
    ThreadPool::Instance()->waitForDone();
    HttpRequestManager::Instance()->clearTask();
}

void HttpRequestManager::set_share_handle(CURL* curl_handle)
{
    if (curl_handle && s_share_handle_)
    {
        curl_easy_setopt(curl_handle, CURLOPT_SHARE, s_share_handle_);
        curl_easy_setopt(curl_handle, CURLOPT_DNS_CACHE_TIMEOUT, 60 * 5);
    }
}

void HttpRequestManager::insertTask(TaskBase* t)
{
    TPLocker locker(m_lock);
    m_map_tasks[t->id()] = t;
}

void HttpRequestManager::removeTask(int task_id)
{
    TPLocker locker(m_lock);

    auto iter = m_map_tasks.find(task_id);
    if (iter != m_map_tasks.end())
    {
        TaskBase* t = iter->second;
        if (t)
        {
            delete t;
        }

        m_map_tasks.erase(iter);
    }
}

void HttpRequestManager::clearTask()
{
    TPLocker locker(m_lock);
    auto iter = m_map_tasks.begin();
    for (; iter != m_map_tasks.end();)
    {
        TaskBase* t = iter->second;
        if (t)
        {
            delete t;
        }

        iter = m_map_tasks.erase(iter);
    }
}

void HttpRequestManager::HttpTaskCallBack::onTaskFinished(int task_id)
{
    HttpRequestManager::Instance()->removeTask(task_id);
}