#pragma once
#include <windows.h>
#include <memory>

class CMutex
{
public:
    CMutex(void);
    ~CMutex(void);

public:
    bool Lock();
    bool UnLock();

private:
    CRITICAL_SECTION m_cs;
};

class CMutexLocker
{
public:
	CMutexLocker(std::shared_ptr<CMutex>& lock)
		: m_lock(lock)
	{
		m_lock->Lock();
	}

	~CMutexLocker()
	{
		m_lock->UnLock();
		m_lock = 0;
	}

private:
	std::shared_ptr<CMutex> m_lock;
};

