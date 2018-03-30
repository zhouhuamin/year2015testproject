// SysMutex.h: interface for the CSysMutex class.
//
//////////////////////////////////////////////////////////////////////

#if !defined _SYS_MUTEX_H_
#define _SYS_MUTEX_H_

#ifdef WIN32
#include <afxmt.h>
#else
#include <pthread.h>
#endif

class CSysMutex
{
public:
    void UnLock();
    void Lock();
    CSysMutex();
    virtual ~CSysMutex();

private:
#ifdef WIN32
    CRITICAL_SECTION m_crtRealDataList;
#else
    pthread_mutex_t m_crtRealDataList;
#endif

};

#endif 
