#pragma once
#include <windows.h>
#include <memory>

class CSLock
{
public:
	CSLock(void);
	~CSLock(void);

public:
	bool lock();
	bool unLock();

private:
	CRITICAL_SECTION m_cs;
};

class CSLocker
{
public:
	CSLocker(std::shared_ptr<CSLock> lock)
		: m_lock(lock)
	{
		m_lock->lock();
	}

	~CSLocker()
	{
		m_lock->unLock();
		m_lock = 0;
	}

private:
	std::shared_ptr<CSLock> m_lock;
};

