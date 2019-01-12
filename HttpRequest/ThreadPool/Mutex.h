#pragma once
#include <windows.h>
#include <memory>

class TPLock
{
public:
	TPLock(void);
	~TPLock(void);

public:
	bool lock();
	bool unLock();

private:
	CRITICAL_SECTION m_cs;
};

class TPLocker
{
public:
	TPLocker(std::shared_ptr<TPLock> lock)
		: m_lock(lock)
	{
		m_lock->lock();
	}

	~TPLocker()
	{
		m_lock->unLock();
		m_lock = 0;
	}

private:
	std::shared_ptr<TPLock> m_lock;
};

