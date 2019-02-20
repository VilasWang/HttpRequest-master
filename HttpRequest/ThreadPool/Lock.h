#pragma once
#include <windows.h>
#include <memory>

//Class CSLock - ¹Ø¼ü¶ÎËø
class CSLock
{
public:
	explicit CSLock();
	~CSLock();

	void lock();
	bool tryLock();
	void unlock();

private:
	CRITICAL_SECTION m_cs;
};

//Class SRWLock - slim ¶ÁÐ´Ëø
class SRWLock
{
public:
	explicit SRWLock();
	~SRWLock();

	void lock(bool bShared = false);
	void unlock();

private:
	SRWLOCK m_lock;
	long m_bSharedLocked;
	long m_bExclusiveLocked;
};

template<class _Lock>
class Locker
{
public:
	explicit Locker(_Lock& lock)
		: m_lock(lock)
	{
		m_lock.lock();
	}

	Locker(_Lock& lock, bool bShared)
		: m_lock(lock)
	{
		m_lock.lock(bShared);
	}

#if _MSC_VER >= 1700
	~Locker() _NOEXCEPT
#else
	~Locker()
#endif
	{
		m_lock.unlock();
	}

#if _MSC_VER >= 1700
	Locker(const Locker&) = delete;
	Locker& operator=(const Locker&) = delete;
#endif

private:
#if _MSC_VER < 1700
	Locker(const Locker&);
	Locker& operator=(const Locker&);
#endif

private:
	_Lock& m_lock;
};