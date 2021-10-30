#pragma once

#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/AtomicRefCounter.h"
#include "Runtime/BaseClasses/BaseObject.h"

class AsyncOperation
{
	typedef void DelayedCall(Object* o, void* userData);
	typedef void CleanupUserData (void* userData);
	
	AtomicRefCounter m_RefCount;
	DelayedCall* m_CoroutineDone;
	CleanupUserData* m_CoroutineCleanup;
	void*        m_CoroutineData;
	PPtr<Object> m_CoroutineBehaviour;
public:
	
	AsyncOperation () { m_CoroutineDone = NULL; }
	virtual ~AsyncOperation ();
	virtual bool IsDone () = 0;
	virtual float GetProgress () = 0;
	
	virtual int GetPriority () { return 0; }
	virtual void SetPriority (int priority) {  }
	
	virtual bool GetAllowSceneActivation () { return true; }
	virtual void SetAllowSceneActivation (bool allow) {  }
	
	void Retain ();
	void Release ();
	
	bool HasCoroutineCallback () { return m_CoroutineDone != NULL; }
	void SetCoroutineCallback (	DelayedCall* func, Object* coroutineBehaviour, void* userData, CleanupUserData* cleanup);

	void InvokeCoroutine ();
	void CleanupCoroutine ();
};
