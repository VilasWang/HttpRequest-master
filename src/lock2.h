#pragma once

#include <windows.h>
#include <memory>

namespace CVC
{
    class Lock
    {
    public:
        Lock();
        ~Lock();

    public:
        void lock();
        void unlock();

    private:
        Lock(const Lock&);
        Lock& operator=(const Lock&);

    private:
        CRITICAL_SECTION m_cs;
    };

    template<class _Lock>
    class Locker2
    {
    public:
        explicit Locker2(_Lock& lock)
            : m_lock(lock)
        {
            m_lock.lock();
        }

        Locker2(_Lock& lock, bool bShared)
            : m_lock(lock)
        {
            m_lock.lock(bShared);
        }

#if _MSC_VER >= 1700
        ~Locker2() _NOEXCEPT
#else
        ~Locker2()
#endif
        {
            m_lock.unlock();
        }

    private:
        Locker2(const Locker2&);
        Locker2& operator=(const Locker2&);

    private:
        _Lock& m_lock;
    };
}
