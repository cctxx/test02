#ifndef __EVENT_H
#define __EVENT_H

// Event synchronization object.

#if SUPPORT_THREADS

#if UNITY_WIN || UNITY_XENON
#	include "Winapi/PlatformEvent.h"
#elif HAS_EVENT_OBJECT
#	include "PlatformEvent.h"
#else
#	include "Semaphore.h"
	typedef Semaphore Event;
#	pragma message("Event implementation missing. Using a Semaphore.")
#endif

#endif // SUPPORT_THREADS

#endif // __EVENT_H
