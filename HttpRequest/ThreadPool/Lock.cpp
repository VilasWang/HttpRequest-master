//#include "stdafx.h"
#include "Lock.h"
#include <winnt.h>

CSLock::CSLock(void)
{
	InitializeCriticalSection(&m_cs);
}

CSLock::~CSLock(void)
{
	DeleteCriticalSection(&m_cs);
}

bool CSLock::lock()
{
	EnterCriticalSection(&m_cs);
	return true;
}

bool CSLock::unLock()
{
	LeaveCriticalSection(&m_cs);
	return true;
}


SRWLock::SRWLock()
	: m_bSharedLocked(FALSE)
	, m_bExclusiveLocked(FALSE)
{
	InitializeSRWLock(&m_lock);
}

SRWLock::~SRWLock()
{
	unLock();
}

bool SRWLock::lock(bool bShared)
{
	if (bShared)
	{
		AcquireSRWLockShared(&m_lock);
		InterlockedExchange(&m_bSharedLocked, TRUE);
		return true;
	}
	else
	{
		AcquireSRWLockExclusive(&m_lock);
		InterlockedExchange(&m_bExclusiveLocked, TRUE);
		return true;
	}
	_ASSERT(false);
	return false;
}

bool SRWLock::unLock()
{
	if (TRUE == InterlockedExchange(&m_bSharedLocked, FALSE))
	{
		ReleaseSRWLockShared(&m_lock);
		return true;
	}
	else if (TRUE == InterlockedExchange(&m_bExclusiveLocked, FALSE))
	{
		ReleaseSRWLockExclusive(&m_lock);
		return true;
	}
	return true;
}