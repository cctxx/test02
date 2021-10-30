#ifndef __SPINLOCK_MUTEX_H
#define __SPINLOCK_MUTEX_H

#if UNITY_OSX

#include <libkern/OSAtomic.h>

class SpinlockMutex : public NonCopyable
{
public:
	
	class AutoLock
	{
	public:
		AutoLock( SpinlockMutex& mutex )
		: m_Mutex(&mutex)
		{
			mutex.Lock();
		}
		
		~AutoLock()
		{
			m_Mutex->Unlock();
		}
		
	private:
		AutoLock(const AutoLock&);
		AutoLock& operator=(const AutoLock&);
		
	private:
		SpinlockMutex*	m_Mutex;
	};
	
	SpinlockMutex()
	{
		m_SpinLock = OS_SPINLOCK_INIT;
	}
	
	~SpinlockMutex()
	{}
	
	void Lock()
	{
		OSSpinLockLock(&m_SpinLock);
	}
	
	void Unlock()
	{
		OSSpinLockUnlock(&m_SpinLock);
	}
	
private:
	
	volatile OSSpinLock m_SpinLock;
};

#else

typedef Mutex SpinlockMutex;

#endif

#endif