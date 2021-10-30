#ifndef __PLATFORMSEMAPHORE_H
#define __PLATFORMSEMAPHORE_H

#if SUPPORT_THREADS

#include "Runtime/Utilities/NonCopyable.h"

class PlatformSemaphore : public NonCopyable
{
	friend class Semaphore;
protected:
	void Create();
	void Destroy();
	
	void WaitForSignal();
	void Signal();
	
private:
	HANDLE	m_Semaphore;
};

	inline void PlatformSemaphore::Create()
	{
#if UNITY_WINRT
		m_Semaphore = CreateSemaphoreExW(NULL, 0, 256, NULL, 0, (STANDARD_RIGHTS_REQUIRED | SEMAPHORE_MODIFY_STATE | SYNCHRONIZE));
#else
		m_Semaphore = CreateSemaphoreA( NULL, 0, 256, NULL );
#endif
	}
	inline void PlatformSemaphore::Destroy(){ if( m_Semaphore ) CloseHandle(m_Semaphore); }
	inline void PlatformSemaphore::WaitForSignal()
	{
#if UNITY_WINRT	// ?!-
		WaitForSingleObjectEx(m_Semaphore, INFINITE, FALSE);
#else
		while (1)
		{
			DWORD result = WaitForSingleObjectEx( m_Semaphore, INFINITE, TRUE );
			switch (result)
			{
			case WAIT_OBJECT_0:
				// We got the signal
				return;
			case WAIT_IO_COMPLETION:
				// Allow thread to run IO completion task
				Sleep(1);
				break;
			default:
				Assert(false);
				break;
			}
		}
#endif
	}
	inline void PlatformSemaphore::Signal() { ReleaseSemaphore( m_Semaphore, 1, NULL ); }

#endif // SUPPORT_THREADS

#endif // __PLATFORMSEMAPHORE_H
