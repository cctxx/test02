#include "UnityPrefix.h"
#include "EventManager.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"

////@TODO: Assert on recursive calls...

EventManager* EventManager::s_Instance = NULL;

EventManager& GetEventManager ()
{
	return *EventManager::s_Instance;
}

void EventManager::StaticInitialize()
{
	s_Instance = UNITY_NEW(EventManager,kMemManager);
}

void EventManager::StaticDestroy()
{
	UNITY_DELETE(s_Instance, kMemManager);
}

static RegisterRuntimeInitializeAndCleanup s_EventManagerCallbacks(EventManager::StaticInitialize, EventManager::StaticDestroy);

EventManager::EventManager ()
: m_EventPool (false, "EventManager", sizeof(EventEntry), 1024 * 4)
#if DEBUGMODE
,	m_InvokingEventList(NULL)
,	m_InvokingEventActiveNode(NULL)
#endif
{
}

EventManager::EventIndex EventManager::AddEvent (EventCallback* callback, void* userData, EventIndex previousIndex)
{
	if (previousIndex == NULL)
	{
		EventIndex event = (EventIndex)m_EventPool.Allocate();
		event->userData = userData;
		event->callback = callback;
		event->next = NULL;
		
		return event;
	}
	else
	{
		EventIndex event = (EventIndex)m_EventPool.Allocate();
		event->callback = callback;
		event->userData = userData;
		event->next = previousIndex;
		
		return event;
	}
}

/// Removes all events with the event index.
void EventManager::RemoveEvent (EventIndex index)
{
	#if DEBUGMODE
	// We can not delete the event which we are currently invoking
	Assert (m_InvokingEventList != index);
	#endif

	while (index != NULL)
	{
		EventIndex next = index->next;
		m_EventPool.Deallocate(index);
		index = next;
	}
}

bool EventManager::HasEvent (const EventIndex index, EventCallback* callback, const void* userData)
{
	EventIndex curIndex = index;
	while (curIndex != NULL)
	{
		if (curIndex->callback == callback && curIndex->userData == userData)
			return true;
		
		curIndex = curIndex->next;
	}
	return false;
}


/// Removes an event with a specific callback & userData
/// Returns the new event or null if no events in that index exist anymore.
EventManager::EventIndex EventManager::RemoveEvent (EventIndex index, EventCallback* callback, void* userData)
{
	EventIndex previousIndex = NULL;
	EventIndex curEvent = index;
	while (curEvent != NULL)
	{
		if (curEvent->callback == callback && curEvent->userData == userData)
		{
			// While invoking we are allowed to remove the event being invoked itself but no other events on the same chain.
			#if DEBUGMODE
			Assert (m_InvokingEventList != index || m_InvokingEventActiveNode == curEvent);
			#endif

			EventIndex nextEvent = curEvent->next;
			m_EventPool.Deallocate(curEvent);

			if (previousIndex)
				previousIndex->next = nextEvent;
			
			if (index == curEvent)
				return nextEvent;
			else
				return index;
		}
		
		previousIndex = curEvent;
		
		curEvent = curEvent->next;
	}
	
	return index;
}

void EventManager::InvokeEvent (EventIndex index, void* senderUserData, int eventType)
{
	#if DEBUGMODE
	GetEventManager().m_InvokingEventList = index;
	#endif

	while (index != NULL)
	{
		EventIndex next = index->next;

		#if DEBUGMODE
		GetEventManager().m_InvokingEventActiveNode = index;
		#endif

		index->callback(index->userData, senderUserData, eventType);
		index = next;
	}

	#if DEBUGMODE
	GetEventManager().m_InvokingEventList = NULL;
	GetEventManager().m_InvokingEventActiveNode = NULL;
	#endif
}


#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"


struct LoggingCounter
{
	int counter;
};

void LoggingCallback (void* userData, void* sender, int type)
{
	LoggingCounter* logging = (LoggingCounter*)userData;
	
	logging->counter++;
}

SUITE (EventsManagerTest)
{
TEST (EventsManager_EventsSimple)
{
	EventManager manager;
	
	LoggingCounter counter1;
	counter1.counter = 0;

	EventManager::EventIndex index = manager.AddEvent (LoggingCallback, &counter1, NULL);
	manager.InvokeEvent(index, NULL, 0);
	CHECK_EQUAL(1, counter1.counter);
}

// Test chaining (But not duplicating)
TEST (EventsManager_EventsChaining)
{
	EventManager manager;
	
	LoggingCounter counter1;
	counter1.counter = 0;
	LoggingCounter counter2;
	counter2.counter = 0;
	LoggingCounter counter3;
	counter3.counter = 0;

	EventManager::EventIndex index;

	// Add chained event (add one duplicate which should not be added or invoked)
	index = manager.AddEvent (LoggingCallback, &counter1, NULL);
	index = manager.AddEvent (LoggingCallback, &counter2, index);
	index = manager.AddEvent (LoggingCallback, &counter3, index);

	manager.InvokeEvent(index, NULL, 0);
	CHECK_EQUAL(1, counter1.counter);
	CHECK_EQUAL(1, counter2.counter);
	CHECK_EQUAL(1, counter3.counter);

	// Remove 1 chained event
	index = manager.RemoveEvent (index, LoggingCallback, &counter2);
	counter1.counter = 0;
	counter2.counter = 0;
	counter3.counter = 0;

	manager.InvokeEvent(index, NULL, 0);
	CHECK_EQUAL(1, counter1.counter);
	CHECK_EQUAL(0, counter2.counter);
	CHECK_EQUAL(1, counter3.counter);
}
}

#endif

