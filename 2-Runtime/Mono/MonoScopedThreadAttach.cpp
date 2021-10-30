/*
 *  MonoScopedThreadAttach.cpp
 *  AllTargets.workspace
 *
 *  Created by Søren Christiansen on 8/23/11.
 *  Copyright 2011 Unity Technologies. All rights reserved.
 *
 */
#include "UnityPrefix.h"
#include "MonoScopedThreadAttach.h"
#include "MonoIncludes.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Mutex.h"

#if SUPPORT_MONO_THREADS

static attached_thread m_AttachedThreads[MAX_ATTACHED_THREADS] = {{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}};
static Mutex mutex;


// call this from the thread you want to attach!
Thread::ThreadID AttachMonoThread(MonoDomain* domain)
{
	Assert(!Thread::CurrentThreadIsMainThread());
	Mutex::AutoLock lock( mutex );
	
	for (int i=0; i < MAX_ATTACHED_THREADS; ++i)
	{
		if (m_AttachedThreads[i].threadID == Thread::GetCurrentThreadID())
		{
			m_AttachedThreads[i].refCount++;
			return m_AttachedThreads[i].threadID;
		}
		else
			if (m_AttachedThreads[i].threadID == 0)
			{
				m_AttachedThreads[i].threadID = Thread::GetCurrentThreadID();
				m_AttachedThreads[i].thread = mono_thread_attach(domain);
				m_AttachedThreads[i].refCount = 1;
				return m_AttachedThreads[i].threadID;
			}
	}
	
	return 0;	
}

// call this from the thread you want to detach!
bool DetachMonoThread(Thread::ThreadID threadID)
{
	Assert(!Thread::CurrentThreadIsMainThread());
	Mutex::AutoLock lock( mutex );
	
	for (int i=0; i < MAX_ATTACHED_THREADS; ++i)
	{
		if (m_AttachedThreads[i].threadID == Thread::GetCurrentThreadID())
		{
			m_AttachedThreads[i].refCount--;
			if (m_AttachedThreads[i].refCount == 0)
			{
				mono_thread_detach(m_AttachedThreads[i].thread);
				m_AttachedThreads[i].threadID = 0;
				m_AttachedThreads[i].thread = 0;
				m_AttachedThreads[i].refCount = 0;
			}
			return true;
		}
	}
	
	return false;	
}

#endif // SUPPORT_MONO_THREADS
