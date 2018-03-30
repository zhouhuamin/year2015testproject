// MutexGuard.h: interface for the CMutexGuard class.
//
//////////////////////////////////////////////////////////////////////

#if !defined _MUTEX_GUARD_H_
#define _MUTEX_GUARD_H_

#include "SysMutex.h"

class CMutexGuard  
{
public:
	CMutexGuard(CSysMutex& lock);
	virtual ~CMutexGuard();

	void Acquire();
	void Release();

protected:
	CSysMutex*  m_pLock; 
};

#endif 
