#ifndef __HTTP_MANAGER_H
#define __HTTP_MANAGER_H

#include <Windows.h>
#include <map>
#include "curl/curl.h"
#include "ThreadPool/ThreadPool.h"
#include "ThreadPool/mutex.h"

class TaskBase;
class HttpRequestManager
{
public:
	~HttpRequestManager();
	static HttpRequestManager* Instance();

protected:
	HttpRequestManager();

private:
	static void set_share_handle(CURL* curl_handle);
	static bool addTask(std::shared_ptr<TaskBase> t, ThreadPool::Priority priority = ThreadPool::Normal);
	static bool abortTask(int taskId);
	static bool abortAllTask();
	static void globalCleanup();

private:
	void insertTask(std::shared_ptr<TaskBase> t);
	void removeTask(int taskId);
	void clearTask();

private:
	class HttpTaskCallBack : public ThreadPool::ThreadPoolCallBack
	{
	public:
		void onTaskFinished(int taskId) override;
	};

private:
	static CURLSH* s_share_handle_;
	std::map<int, std::shared_ptr<TaskBase>> m_map_tasks;
	HttpTaskCallBack* m_callback;
	std::shared_ptr<TPLock> m_lock;

	friend class CURLWrapper;
	friend class HttpRequest;
};

#endif //__HTTP_MANAGER_H

