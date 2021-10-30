#include "UnityPrefix.h"

#if SUPPORT_THREADS

#include "Mutex.h"

#ifndef THREAD_LOCK_DEBUG
#define THREAD_LOCK_DEBUG 0
#endif

Mutex::Mutex()
{
}

Mutex::~Mutex () 
{
}

bool Mutex::IsLocked()
{
	if(m_Mutex.TryLock())
	{
		Unlock();
		return false;
	}
	else 
	{
		return true;
	}
}

void Mutex::BlockUntilUnlocked() 
{ 
	Lock();
	Unlock();
}

void Mutex::Lock()
{
#if THREAD_LOCK_DEBUG
	m_PerThreadLockDepth.SetIntValue(m_PerThreadLockDepth.GetIntValue() + 1);
#endif
	m_Mutex.Lock();
}

void Mutex::Unlock()
{
#if THREAD_LOCK_DEBUG
	AssertIf(m_PerThreadLockDepth.GetIntValue() == 0);
	m_PerThreadLockDepth.SetIntValue(m_PerThreadLockDepth.GetIntValue() - 1);
#endif
	m_Mutex.Unlock();
}

bool Mutex::TryLock()
{
#if THREAD_LOCK_DEBUG
	if (m_Mutex.TryLock())
	{
		m_PerThreadLockDepth.SetIntValue(m_PerThreadLockDepth.GetIntValue() + 1);
		return true;
	}
	else
		return false;
#else
	return m_Mutex.TryLock();
#endif
}

#endif // SUPPORT_THREADS ; has dummy mutex implemented in headerfile
