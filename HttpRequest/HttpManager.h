#ifndef __HTTP_MANAGER_H
#define __HTTP_MANAGER_H

#include <Windows.h>
#include <map>
#include "curl/curl.h"
#include "ThreadPool/ThreadPool.h"
#include "ThreadPool/cslock.h"
#include "HttpReply.h"

class TaskBase;
// class HttpManager - Httpπ‹¿Ì¿‡
class HttpManager
{
public:
	~HttpManager();
	HttpManager(const HttpManager &) = delete;
	HttpManager &operator=(const HttpManager &) = delete;

	static HttpManager* globalInstance();
	static void globalInit();
	static void globalCleanup();

	void addReply(std::shared_ptr<HttpReply> reply);
	void removeReply(int);
	std::shared_ptr<HttpReply> takeReply(int);
	std::shared_ptr<HttpReply> getReply(int);

protected:
	explicit HttpManager();

private:
	static void set_share_handle(CURL* curl_handle);
	static bool addTask(std::shared_ptr<TaskBase> t, ThreadPool::Priority priority = ThreadPool::Normal);
	static bool abortTask(int taskId);
	static bool abortAllTask();
	void clearReply();

private:
	static CURLSH* s_share_handle_;
	std::map<int, std::shared_ptr<HttpReply>> m_map_replys;
	std::shared_ptr<CSLock> m_lock;

	friend class CURLWrapper;
	friend class HttpRequest;
};

#endif //__HTTP_MANAGER_H

