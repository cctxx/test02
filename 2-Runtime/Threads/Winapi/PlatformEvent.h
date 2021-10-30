#ifndef __PLATFORMEVENT_H
#define __PLATFORMEVENT_H

// Event synchronization object.

#if SUPPORT_THREADS

#include "Runtime/Utilities/NonCopyable.h"

class Event : public NonCopyable
{
public:
	explicit Event();
	~Event();

	void WaitForSignal();
	void Signal();

private:
	HANDLE m_Event;
};

inline Event::Event()
{
#if UNITY_WINRT
	m_Event = CreateEventExW(nullptr, nullptr, 0, 0);	// ?!-
#else
	m_Event = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif
}

inline Event::~Event()
{
	if (m_Event != NULL)
		CloseHandle(m_Event);
}

inline void Event::WaitForSignal()
{
#if UNITY_WINRT
	WaitForSingleObjectEx(m_Event, INFINITE, FALSE);	// ?!-
#else
	while (1)
	{
		DWORD result = WaitForSingleObjectEx(m_Event, INFINITE, TRUE);
		switch (result)
		{
		case WAIT_OBJECT_0:
			// We got the event
			return;
		case WAIT_IO_COMPLETION:
			// Allow thread to run IO completion task
			Sleep(1);
			break;
		default:
			Assert (false);
			break;
		}
	}
#endif
}

inline void Event::Signal()
{
	SetEvent(m_Event);
}

#endif // SUPPORT_THREADS

#endif // __PLATFORMEVENT_H
