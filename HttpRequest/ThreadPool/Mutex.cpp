#include "Mutex.h"

TPLock::TPLock(void)
{
	InitializeCriticalSection(&m_cs);
}

TPLock::~TPLock(void)
{
	DeleteCriticalSection(&m_cs);
}

bool TPLock::lock()
{
	EnterCriticalSection(&m_cs);
	return true;
}

bool TPLock::unLock()
{
	LeaveCriticalSection(&m_cs);
	return true;
}
