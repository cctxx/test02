#ifndef CALLDELAYED_H
#define CALLDELAYED_H

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Utilities/MemoryPool.h"
#include <set>



/// Delayed call is called when the specified time has been reached.
/// o is always non-NULL
typedef void DelayedCall(Object* o, void* userData);
/// CleanupUserData should be used to prevent leaking userData
/// CleanupUserData is called whenever a registered callback is erased.
/// Doing cleanup inside DelayedCall doesnt work since the object might get destroyed or the scene unloaded before a pending function is called
/// CleanupUserData is called only when userData != NULL
typedef void CleanupUserData (void* userData);

/** Class to call functions delayed in time
 */
class DelayedCallManager : public GlobalGameManager
{
	public:
	REGISTER_DERIVED_CLASS (DelayedCallManager, GlobalGameManager)

	enum  {
		kRunFixedFrameRate = 1 << 0,
		kRunDynamicFrameRate = 1 << 1,
		kRunStartupFrame = 1 << 2,
		kWaitForNextFrame = 1 << 3,
		kAfterLoadingCompleted = 1 << 4,
		kEndOfFrame = 1 << 5
	};

	DelayedCallManager (MemLabelId label, ObjectCreationMode mode);
	// virtual ~DelayedCallManager (); declared-by-macro

	/// Time is the time from now we need to exceed to call the function
	/// repeatRate determines at which intervals the call should be repeated. If repeat rate is 0.0F it will not repeat. If it is -1.0 it will repeat but you must have kWaitForNextFrame enabled
	friend void CallDelayed (DelayedCall *func, PPtr<Object> o, float time, void* userData, float repeatRate, CleanupUserData* cleanup, int mode);
	friend void CallDelayedAfterLoading  (DelayedCall *func, PPtr<Object> o, void* userData);

	/// Cancels all CallDelayed	functions on object o if
	/// - the callback is the same
	/// - ShouldCancelCall returns true. (callBackUserData is the userdata stored with CallDelayed. cancelUserData is cancelUserData)
	typedef bool ShouldCancelCall (void* callBackUserData, void* cancelUserdata);
	void CancelCallDelayed (PPtr<Object> o, DelayedCall* callback, ShouldCancelCall* shouldCancel, void* cancelUserData);
	void CancelCallDelayed2 (PPtr<Object> o, DelayedCall* callback, DelayedCall* otherCallback);
	void CancelAllCallDelayed( PPtr<Object> o );
	
	bool HasDelayedCall (PPtr<Object> o, DelayedCall* callback, ShouldCancelCall* shouldCancel, void* cancelUserData);

	virtual void Update (int mask);
	
	void ClearAll ();
	
	int GetNumCallObjects() const { return m_CallObjects.size(); }
	
	private:
	
	/// Struct used to store which functions to execute on which objects.
	struct Callback {
		float time;
		int frame;
		float repeatRate;
		bool repeat;
		void* userData;
		DelayedCall *call;			///< The function call to execute.
		CleanupUserData *cleanup;
		PPtr<Object> object;		///< The object to pass to m_Call.
		int mode;
		int timeStamp;
		
		friend bool operator < (const Callback& lhs, const Callback& rhs) { return lhs.time < rhs.time; }
	};

#if ENABLE_CUSTOM_ALLOCATORS_FOR_STDMAP
	typedef std::multiset<Callback, std::less<Callback>, memory_pool<Callback> > Container;
#else
	typedef std::multiset<Callback, std::less<Callback> > Container;
#endif

	void Remove (const Callback& cb, Container::iterator i);	
	void RemoveNoCleanup (const Callback& cb, Container::iterator i);
	Container m_CallObjects;
	Container::iterator m_NextIterator;
	int m_TimeStamp;
};

/// Calls func in time seconds from now. When o is NULL when the call happens, only cleanup is called.
/// The function call is repeated at repeatRate if repeatRate is not 0.0F
/// cleanup is called whenever userData is non-NULL and a callback is removed (After calling of the delay function without repeat, object was destroyed or Shutdown of a scene)
void CallDelayed (DelayedCall *func, PPtr<Object> o, float time = -1.0F, void* userData = NULL, float repeatRate = 0.0F, CleanupUserData* cleanup = NULL, int mode = DelayedCallManager::kRunDynamicFrameRate | DelayedCallManager::kRunFixedFrameRate);
inline void CallDelayedAfterLoading (DelayedCall *func, PPtr<Object> o, void* userData = NULL)
{
	CallDelayed(func, o, -1.0F, userData, 0.0F, NULL, DelayedCallManager::kAfterLoadingCompleted);
}


DelayedCallManager& GetDelayedCallManager ();

#endif
