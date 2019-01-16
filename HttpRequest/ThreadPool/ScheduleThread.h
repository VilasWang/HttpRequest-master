#pragma once
#ifndef SCHEDULETHREAD_H
#define SCHEDULETHREAD_H

#include <windows.h>
#include <atomic>
#include <mutex>

class ScheduleThread
{
public:
	explicit ScheduleThread();
	virtual ~ScheduleThread();
	ScheduleThread(const ScheduleThread &) = delete;  
	ScheduleThread &operator=(const ScheduleThread &) = delete;

	bool start();
	void quit();
	bool wait(unsigned long time = ULONG_MAX);
	void wakeAll();
	void wakeONe();

	const UINT threadId() const { return m_nThreadID; }

protected:
	virtual void run();
	virtual void onBeforeExec() {}
	virtual void onBeforeExit() {}

private:
	static unsigned __stdcall ThreadFunc(LPVOID pParam);
	void switchThread(UINT threadId);

private:
	HANDLE m_hThread;
	unsigned m_nThreadID;
	std::atomic<bool> m_bExit;
	std::atomic<bool> m_bRunning;
	std::mutex m_mutex;
	std::condition_variable m_task_cv;//Ìõ¼þ×èÈû
};

#endif //SCHEDULETHREAD_H