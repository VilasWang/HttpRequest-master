#include "Mutex.h"

CMutex::CMutex(void)
{
    InitializeCriticalSection(&m_cs);
}

CMutex::~CMutex(void)
{
    DeleteCriticalSection(&m_cs);
}

bool CMutex::Lock()
{
    EnterCriticalSection(&m_cs);
    return true;
}

bool CMutex::UnLock()
{
    LeaveCriticalSection(&m_cs);
    return true;
}
