#include "UnityPrefix.h"
#include "CallDelayed.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Profiler/Profiler.h"

DelayedCallManager::DelayedCallManager (MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{
	m_NextIterator = m_CallObjects.end();
}

DelayedCallManager::~DelayedCallManager () {
	ClearAll ();
}

void CallDelayed (DelayedCall *func, PPtr<Object> o, float time, void* userData, float repeatRate, CleanupUserData* cleanup, int mode)
{
	DelayedCallManager::Callback callback;
	if (time == 0.0F)
		time = -1.0F;
		
	callback.time = time + GetCurTime ();
	callback.frame = -1;
	if (mode & DelayedCallManager::kWaitForNextFrame)
		callback.frame = GetTimeManager().GetFrameCount() + 1;
	callback.repeatRate = repeatRate;
	callback.repeat = repeatRate != 0.0F;
	AssertIf (callback.repeat && repeatRate < 0.00001F && (mode & DelayedCallManager::kWaitForNextFrame) == 0);
	callback.userData = userData;
	callback.call = func;
	callback.cleanup = cleanup;
	callback.object = o;
	callback.mode = mode;
	callback.timeStamp = GetDelayedCallManager ().m_TimeStamp;
	GetDelayedCallManager ().m_CallObjects.insert (callback);
}

void DelayedCallManager::CancelCallDelayed (PPtr<Object> o, DelayedCall* callback, ShouldCancelCall* shouldCancel, void* userData)
{
	Container::iterator next;
	for (Container::iterator i=m_CallObjects.begin ();i != m_CallObjects.end ();i=next)
	{
		next = i; next++;
		Callback &cb = const_cast<Callback&> (*i);
		if (cb.object != o || callback != cb.call)
			continue;
		
		if (shouldCancel == NULL || shouldCancel (cb.userData, userData))
			Remove (cb, i);
	}
}

void DelayedCallManager::CancelCallDelayed2 (PPtr<Object> o, DelayedCall* callback, DelayedCall* otherCallback)
{
	Container::iterator next;
	for (Container::iterator i=m_CallObjects.begin ();i != m_CallObjects.end ();i=next)
	{
		next = i; next++;
		Callback &cb = const_cast<Callback&> (*i);
		if (cb.object == o && (callback == cb.call || cb.call == otherCallback))
			Remove (cb, i);
	}
}

void DelayedCallManager::CancelAllCallDelayed( PPtr<Object> o )
{
	Container::iterator next;
	for (Container::iterator i=m_CallObjects.begin ();i != m_CallObjects.end ();i=next)
	{
		next = i; next++;
		Callback &cb = const_cast<Callback&> (*i);
		if (cb.object != o)
			continue;
		
		Remove (cb, i);
	}	
}

bool DelayedCallManager::HasDelayedCall (PPtr<Object> o, DelayedCall* callback, ShouldCancelCall* shouldCancel, void* cancelUserData)
{
	for (Container::iterator i=m_CallObjects.begin ();i != m_CallObjects.end ();i++)
	{
		Callback &cb = const_cast<Callback&> (*i);
		if (cb.object != o || callback != cb.call)
			continue;
		
		if (shouldCancel == NULL || shouldCancel (cb.userData, cancelUserData))
			return true;
	}
	return false;
}


inline void DelayedCallManager::Remove (const Callback& cb, Container::iterator i)
{
	CleanupUserData* cleanup = cb.cleanup;
	void* userData = cb.userData;

	if (m_NextIterator != i)
		m_CallObjects.erase (i);
	else
	{
		m_NextIterator++;
		m_CallObjects.erase (i);
	}

	if (cleanup && userData)
		cleanup (userData);
}

inline void DelayedCallManager::RemoveNoCleanup (const Callback& cb, Container::iterator i)
{
	if (m_NextIterator != i)
		m_CallObjects.erase (i);
	else
	{
		m_NextIterator++;
		m_CallObjects.erase (i);
	}
}

//PROFILER_INFORMATION(gDelayedCallProfile, "Coroutines & Delayed Call", kProfilerOther)

// Call all delayed functions when the time has come.
void DelayedCallManager::Update (int modeMask)
{
//	PROFILER_AUTO(gDelayedCallProfile, NULL)
	
	// For robustness we are using a iterator that is stored in the manager and when Remove is called 
	// it makes sure that the iterator is set to the next element so that we never end up with a stale ptr
	float time = GetCurTime();
	int frame = GetTimeManager().GetFrameCount();
	Container::iterator i = m_CallObjects.begin ();
	m_TimeStamp++;

	while (i !=  m_CallObjects.end () && i->time <= time)
	{
		m_NextIterator = i;	m_NextIterator++;
		
		Callback &cb = const_cast<Callback&> (*i);
		//- Make sure the mask matches.
		//- We never execute delayed calls that are added during the DelayedCallManager::Update function
		if ((cb.mode & modeMask) && cb.timeStamp != m_TimeStamp && cb.frame <= frame)
		{
			// avoid loading stuff from persistent manager in the middle of async loading
			Object *o = Object::IDToPointer (cb.object.GetInstanceID ());

			if (o)
			{
				void* userData = cb.userData;
				DelayedCall* callback = cb.call;
				// Cleanup and Removal is a bit hard
				// Problems are
				// - CancelCall might be called from inside the callback so the best way is to remove the callback structure before.
				// - of course we still need the user data to be deallocated calling the callback not before
				if (!cb.repeat)
				{
					// Remove callback structure from set
					CleanupUserData* cleanup = cb.cleanup;
					RemoveNoCleanup (cb, i);
					//call callback
					callback (o, userData);
					//afterwards cleanup userdata
					if (cleanup && userData)
						cleanup (userData);
				}
				else
				{
					// Advance time and reinsert (We dont call the cleanup function because we are repeating the call.
					// It can only be canceleld by CancelDelayCall)
					cb.time += cb.repeatRate;
					if (cb.mode & DelayedCallManager::kWaitForNextFrame)
						cb.frame = GetTimeManager().GetFrameCount() + 1;
					
					m_CallObjects.insert (cb);
					RemoveNoCleanup (cb, i);
					// call callback
					callback (o, userData);
				}
			}
			else
				Remove (cb, i);
		}
		
		i = m_NextIterator;		
	}
}

void DelayedCallManager::ClearAll ()
{
	for (Container::iterator i=m_CallObjects.begin ();i != m_CallObjects.end ();i++)
	{
		Callback &cb = const_cast<Callback&> (*i);
		if (cb.cleanup && cb.userData)
			cb.cleanup (cb.userData);
	}
	m_CallObjects.clear ();
}

IMPLEMENT_CLASS (DelayedCallManager)
GET_MANAGER (DelayedCallManager)
