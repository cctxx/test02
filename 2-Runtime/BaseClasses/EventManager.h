#pragma once

#include "Runtime/Utilities/MemoryPool.h"

typedef void EventCallback (void* userData, void* sender, int eventType);


// Small event entry. Keep this tight.
struct EventEntry
{
	void*          userData;
	EventEntry*    next;
	EventCallback* callback;
};

class EventManager
{
public:
	typedef EventEntry* EventIndex;

private:
	////@TODO: Memory pool has a minimum size of 32 bytes. This one fits in 12. WTF???
	MemoryPool m_EventPool;

	static EventManager* s_Instance;
	friend EventManager& GetEventManager ();

	#if DEBUGMODE
	EventIndex m_InvokingEventList;
	EventIndex m_InvokingEventActiveNode;
	#endif

public:
	EventManager ();

	static void StaticInitialize ();
	static void StaticDestroy ();
	
	/// Adds an event
	/// If there is already a previous event registered, it will chain them.
	/// The reference to the event is the returned eventIndex
	EventIndex AddEvent (EventCallback* callback, void* userData, EventIndex previousIndex);
	
	/// Removes all events with the event index.
	void RemoveEvent (EventIndex index);

	/// Removes an event with a specific callback & userData
	/// Returns the new event or null if no events in that index exist anymore.
	/// AddEvent and RemoveEvent calls must be balanced.
	EventIndex RemoveEvent (EventIndex index, EventCallback* callback, void* userData);

	/// Does the event with that specific callback and userData exist?
	static bool HasEvent (const EventIndex index, EventCallback* callback, const void* userData);
	
	static void InvokeEvent (EventIndex index, void* sender, int eventType);
};

EventManager& GetEventManager ();

