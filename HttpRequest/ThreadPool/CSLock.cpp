//#include "stdafx.h"
#include "CSLock.h"

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
