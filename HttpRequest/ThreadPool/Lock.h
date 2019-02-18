#pragma once
#include <windows.h>
#include <memory>

//Class CSLock - ¹Ø¼ü¶ÎËø
class CSLock
{
public:
	CSLock(void);
	~CSLock(void);

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

//Class SRWLock - slim ¶ÁÐ´Ëø
class SRWLock
{
public:
	SRWLock();
	~SRWLock();

	bool lock(bool bShared = false);
	bool unLock();

private:
	SRWLOCK m_lock;
	long m_bSharedLocked;
	long m_bExclusiveLocked;
};

