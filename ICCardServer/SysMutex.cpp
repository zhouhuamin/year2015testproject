// SysMutex.cpp: implementation of the CSysMutex class.
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
#include "stdafx.h"
#else
#endif

#include "SysMutex.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSysMutex::CSysMutex()
{
#ifdef WIN32
    InitializeCriticalSection(&m_crtRealDataList);
#else
    pthread_mutex_init(&m_crtRealDataList, NULL);
#endif

}

CSysMutex::~CSysMutex()
{
#ifdef WIN32
    DeleteCriticalSection(&m_crtRealDataList);
#else
    pthread_mutex_destroy(&m_crtRealDataList);
#endif


}

void CSysMutex::Lock()
{
#ifdef WIN32
    EnterCriticalSection(&m_crtRealDataList);
#else
    pthread_mutex_lock(&m_crtRealDataList);
#endif
}

void CSysMutex::UnLock()
{
#ifdef WIN32
    LeaveCriticalSection(&m_crtRealDataList);
#else
    pthread_mutex_unlock(&m_crtRealDataList);
#endif
}
