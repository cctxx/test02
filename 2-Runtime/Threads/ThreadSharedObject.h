#ifndef THREAD_SHARED_OBJECT_H
#define THREAD_SHARED_OBJECT_H

#include "Runtime/Utilities/NonCopyable.h"
#include "Runtime/Threads/AtomicOps.h"

class ThreadSharedObject : public NonCopyable
{
public:
	void AddRef() const	{ AtomicIncrement(&m_RefCount); }
	void Release() const { if (AtomicDecrement(&m_RefCount) == 0) delete this; }
	void Release(MemLabelId label) const 
	{
		if (AtomicDecrement(&m_RefCount) == 0) 
		{
			this->~ThreadSharedObject(); 
			UNITY_FREE(label,const_cast<ThreadSharedObject*>(this));
		}
	}
	int  GetRefCount() const { return m_RefCount; }

protected:
	ThreadSharedObject(int refs = 1) : m_RefCount(refs) {}
	virtual ~ThreadSharedObject() {}

private:
	volatile mutable int m_RefCount;
};

#endif
