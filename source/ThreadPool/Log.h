﻿#ifndef __CLOG_H__
#define __CLOG_H__
#pragma once

#include <stdio.h>
#include <varargs.h>

#define MaxLogSize 128
namespace Log
{
    inline void Debug(char *format, ...)
    {
        char buf[MaxLogSize] = { 0 };

        va_list args;
        va_start(args, format);
        int len = _vsnprintf_s(buf, _countof(buf) - 1, format, args);
        va_end(args);

        if (len != -1)
        {
            buf[MaxLogSize-1] = '\0';
            OutputDebugStringA(buf);
        }
    }

    inline void DebugW(wchar_t *format, ...)
    {
        wchar_t buf[MaxLogSize] = { 0 };

        va_list args;
        va_start(args, format);
        int len = _vsnwprintf_s(buf, _countof(buf) - 1, format, args);
        va_end(args);

        if (len != -1)
        {
            buf[MaxLogSize-1] = '\0';
            OutputDebugStringW(buf);
        }
    }
}

#ifdef _DEBUG
#ifdef USE_LOG_DEBUG

#ifndef LOG_DEBUG
#define LOG_DEBUG Log::Debug
#endif // !LOG_DEBUG

#ifndef LOG_DEBUGW
#define LOG_DEBUGW Log::DebugW
#endif // !LOG_DEBUGW

#else

#ifndef LOG_DEBUG
#define LOG_DEBUG __noop
#endif // !LOG_DEBUG

#ifndef LOG_DEBUGW
#define LOG_DEBUGW __noop
#endif // !LOG_DEBUGW

#endif

#else

#ifndef LOG_DEBUG
#define LOG_DEBUG __noop
#endif // !LOG_DEBUG

#ifndef LOG_DEBUGW
#define LOG_DEBUGW __noop
#endif // !LOG_DEBUGW

#endif


#endif
