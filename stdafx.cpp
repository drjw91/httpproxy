// stdafx.cpp : source file that includes just the standard includes
//	localproxy.pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

#if (_ATL_VER < 0x0700)
#include <atlimpl.cpp>
#endif //(_ATL_VER < 0x0700)

HWND g_HWND = NULL;

void Log(char* szlog)
{
    if (g_HWND && szlog && szlog[0])
    {
        int nsize = strlen(szlog)+1;
        char *log = new(std::nothrow) char[nsize];
        if(log)
        {
            memset(log,0,nsize);
            memcpy(log,szlog,nsize-1 );
            if(!::PostMessage(g_HWND, WM_LOGINFO, 0, (LPARAM)(log)))
            {
                delete[] log;
                log = NULL;
            }
        }
    }
}

void LogThread( int n )
{
    if (g_HWND )
    {
        ::PostMessage(g_HWND, WM_LOGINFO, 1, (LPARAM)(n));
    }
}
