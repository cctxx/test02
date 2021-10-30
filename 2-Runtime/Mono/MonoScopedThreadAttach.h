/*
 *  MonoScopedThreadAttach.h
 *  AllTargets.workspace
 *
 *  Created by Søren Christiansen on 8/23/11.
 *  Copyright 2011 Unity Technologies. All rights reserved.
 *
 */
#ifndef __UNITY_MONOSCOPEDTHREADATTACH_H__
#define __UNITY_MONOSCOPEDTHREADATTACH_H__

#if SUPPORT_MONO_THREADS

#include "Runtime/Threads/Thread.h"

#define MAX_ATTACHED_THREADS 4

struct MonoDomain;
struct MonoThread;

Thread::ThreadID AttachMonoThread(MonoDomain* domain);
bool DetachMonoThread(Thread::ThreadID threadID);

struct attached_thread
{
	Thread::ThreadID threadID;
	MonoThread* thread;		
	int refCount;
};

struct ScopedThreadAttach
{
	ScopedThreadAttach(MonoDomain* domain) : threadID(0) 
	{
		Assert(domain);
		if (!Thread::CurrentThreadIsMainThread()) 
			threadID = AttachMonoThread(domain);
	}
	~ScopedThreadAttach() 
	{
		if (threadID)
			DetachMonoThread(threadID);
	}
	
private:
	ScopedThreadAttach(const ScopedThreadAttach& other);
	void operator=(const ScopedThreadAttach& other);
	Thread::ThreadID threadID;
};

#endif // SUPPORT_MONO_THREADS
#endif // __UNITY_MONOSCOPEDTHREADATTACH_H__
