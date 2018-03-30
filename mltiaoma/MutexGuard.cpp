// MutexGuard.cpp: implementation of the CMutexGuard class.
//
//////////////////////////////////////////////////////////////////////

#include "MutexGuard.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMutexGuard::CMutexGuard(CSysMutex& lock):m_pLock(&lock)
{
	Acquire();
}

CMutexGuard::~CMutexGuard()
{
	Release();
}

void CMutexGuard::Acquire()
{
	m_pLock->Lock();
}

void CMutexGuard::Release()
{
	m_pLock->UnLock();
}
