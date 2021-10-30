#include "UnityPrefix.h"

/*
 * TouchPhaseEmulation is layer between an 'event-based' touch OS (Android, Metro etc) and our phase-based script API (similar to iOS).
 *
 * The core logic of this layer is in DispatchTouchEvent, which handles mapping and collapsing of events to a frame-discrete phase.
 */

#define DEBUG_TOUCH_EMU	(DEBUGMODE && 0)

#include "TouchPhaseEmulation.h"

#if UNITY_BLACKBERRY
		static const int touchTimeout = 400;
#else
		static const int touchTimeout = 150;
#endif

// *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***

class TouchImpl : public Touch
{
	enum { kEmptyTouchId = ~0UL };

public:
	TouchImpl ()
	{
		clear ();
	}

	long long timestamp;     // in milliseconds
	UInt32 pointerId;        // matches OS pointerId
	size_t frameToReport;    // frame # for this event to be reported. Should
	                         // only be =gFrameCount or =gFrameCount+1
	size_t frameBegan;       // frame # when BEGIN event was received
	UInt32 endPhaseInQueue;  // acts both as a bool to indicate that this event
	                         // has already received an END/CANCEL from OS and
	                         // will be reported to scripts next frame, and as
	                         // a container to hold the actual value: was it
	                         // END or CANCEL?

	void setDeltaTime (long long newTimestamp)
	{
		if (timestamp == 0)
			return;

		deltaTime = (newTimestamp - timestamp) / 1000.0f;
	}

	void setDeltaPos (Vector2f const& newPos)
	{
		if (CompareApproximately (pos, Vector2f::zero))
			return;

		deltaPos = newPos - pos;
	}

	bool isMultitap (long long newTimestamp, Vector2f const& newPos, float screenDPI)
	{
		static const float tapZoneRadiusCM = 0.4f;	// 4mm
		static const float cmToInch = 0.393701f;
		static const float multitapRadiusPixels = screenDPI * tapZoneRadiusCM * cmToInch;
		static const float multitapRadiusSqr = multitapRadiusPixels * multitapRadiusPixels;

		return newTimestamp - timestamp < touchTimeout
		       && SqrMagnitude (pos - newPos) < multitapRadiusSqr;
	}

	void setTapCount (long long newTimestamp, Vector2f const &newPos, float screenDPI)
	{
		if (isMultitap (newTimestamp, newPos, screenDPI))
			++tapCount;
		else
			tapCount = 1;
	}

	void init(size_t _pointerId, Vector2f _pos, TouchPhaseEmulation::TouchPhase _phase,
				long long _timestamp, size_t currFrame)
	{
		pointerId		= _pointerId;
		pos				= _pos;
		rawPos			= _pos;
		phase			= _phase;
		frameBegan		= currFrame;
		timestamp		= _timestamp;
		frameToReport	= currFrame;
	}

	void clear ()
	{
		id = kEmptyTouchId;
		phase = TouchPhaseEmulation::kTouchCanceled;
		endPhaseInQueue = 0;
		deltaPos = Vector2f (0.0f, 0.0f);
		deltaTime = 0.0f;
		frameToReport = 0;
		frameBegan = 0;
		tapCount = 0;
		rawPos = pos = Vector2f (0.0f, 0.0f);
		timestamp = 0;
		pointerId = kEmptyTouchId;
	}

	bool isOld (size_t frame) const
	{
		return frameToReport < frame;
	}

	bool isEmpty () const
	{
		return id == kEmptyTouchId;
	}

	bool isFinished () const
	{
		return !isEmpty () && IsEnd (phase);
	}

	bool willBeFinishedNextFrame () const
	{
		return !isEmpty () && IsEnd (endPhaseInQueue);
	}

	bool isNow (size_t frame) const
	{
		return frameToReport == frame;
	}

	static bool IsBegin (size_t phase)
	{
		return phase == TouchPhaseEmulation::kTouchBegan;
	}

	static bool IsTransitional (size_t phase)
	{
		return phase == TouchPhaseEmulation::kTouchMoved || phase == TouchPhaseEmulation::kTouchStationary;
	}

	static bool IsEnd (size_t phase)
	{
		return phase == TouchPhaseEmulation::kTouchEnded || phase == TouchPhaseEmulation::kTouchCanceled;
	}


#if DEBUG_TOUCH_EMU
	void dump (TouchImpl* gAllTouches)
	{
		size_t index = this - gAllTouches;
		char const* phaseNames[] = {
			"<Began>",
			"<Moved>",
			"<Stationary>",
			"<Ended>",
			"<Canceled>",
		};

		char const *fmt =
		"T[%02d]={fid=%d, pid=%d, phase=%s, p=(%3.1f,%3.1f), dp=(%3.1f,%3.1f),\n"
		"       tm=%lld, dtm=%f, endPhaseQ=%d, fbeg=%d, frep=%d, tcnt=%d}\n";

		printf_console (fmt, index, id, pointerId, phaseNames[phase], pos.x, pos.y,
		                deltaPos.x, deltaPos.y, timestamp, deltaTime,
		                endPhaseInQueue, frameBegan, frameToReport, tapCount);
	}
#endif
};

// *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***

TouchPhaseEmulation::TouchPhaseEmulation(float screenDPI, bool singleTouchDevice)
:	m_AllocatedFingerIDs(0)
,	m_FrameCount(0)
,	m_ScreenDPI(screenDPI)
,	m_IsMultiTouchEnabled(!singleTouchDevice)
,	m_IsSingleTouchDevice(singleTouchDevice)
{
	m_TouchSlots = new TouchImpl[kMaxTouchCount];
	InitTouches();
}

TouchPhaseEmulation::~TouchPhaseEmulation()
{
	delete [] m_TouchSlots;
}

void TouchPhaseEmulation::InitTouches ()
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		m_TouchSlots[i].clear();
	}
	m_AllocatedFingerIDs = 0;

	m_FrameCount = 1;
}

void TouchPhaseEmulation::PreprocessTouches ()
{
	DiscardRedundantTouches();
}

void TouchPhaseEmulation::PostprocessTouches ()
{
#if DEBUG_TOUCH_EMU
	DumpAll ();
#endif
	++m_FrameCount;
	UpdateActiveTouches();
}


bool TouchPhaseEmulation::IsExistingTouch( int pointerId )
{
	TouchImpl*	matchingSlots[kMaxTouchCount];
	const size_t slotsFound = FindByPointerId(matchingSlots, pointerId);

	for (size_t i = 0; i < slotsFound; ++i)
		if (matchingSlots[i])
			return true;

	return false;
}

void TouchPhaseEmulation::AddTouchEvent (int pointerId, float x, float y, TouchPhase newPhase, long long timestamp)
{
	Vector2f pos = Vector2f (x, y);

#if UNITY_WINRT
	// [Metro] With one finger touching; that touch pointerId is usually > 0
	if (!m_IsMultiTouchEnabled && GetTouchCount() > 0 && !IsExistingTouch(pointerId))
		return;
#else
	if (!m_IsMultiTouchEnabled && pointerId > 0)
		return;
#endif

	DispatchTouchEvent (pointerId, pos, static_cast<TouchPhase> (newPhase), timestamp, m_FrameCount);
}

void TouchPhaseEmulation::DispatchTouchEvent (size_t pointerId, Vector2f pos, TouchPhase newPhase, long long timestamp, size_t currFrame)
{
	// Brief terminology legend:
	//		action			The OS level indication of what happened in a particular touch event; DOWN, MOVE, UP etc.
	//		phase			Logical state of a touch, emulated to the script side; Began/Moved/Ended.
	//						Phase is considered constant per frame, and all intra-frame actions are collapsed to a single phase.
	//						If two actions can't be collapsed (like UP/DOWN), then the DOWN action will be delayed one frame.
	//		pointerId		The ID given to a touch event by the OS. These IDs can be reused if touches end/start within the same frame.
	//		fingerId		The ID we give a touch when presented to the script side. Also known as 'id' in the Touch struct.
	//		touch event		OS level touch information
	//		touch slot		Script level touch information.

	// Step 1: Determine if already tracking this 'pointerId'
	//			We do this by looping through all touch slots while looking for an active touch with matching 'pointerId'.
	//			If a slot with phase == Ended has been inactive for 150ms => set it to Inactive
	// Step 2: Determine if an old touch slot should be updated with the new event information, or if a new touch slot should be allocated.
	//			If no touch slot was found in Step 1 => allocate a new touch slot. Skip to step 4.
	//			If only a singular touch slot was found in Step 1 => use that slot. Skip to step 4.
	//			If multiple slots were found in Step 1 => determine which slot to use, or if another slot needs to be allocated.
	// Step 3: Determine which active slot to use, or allocate a new.
	//			If phase == Began : for each slot with phase == Ended, consider event to be a multi-tap by comparing position and timestamp.
	//			If phase != Began : find slot with phase != Ended (should only be one!)
	//			If no slot matches : allocate a new slot with fingerId = highestFingerIdUsed + 1.
	// Step 4: Initialize/update the touch slot based on current and old touch phase.
	//			If new phase == Began and old phase == Ended => increase tap count.
	//			If new phase == Began => try to compact finger id
	//			If new phase == Ended and old phase == Began on the same frame => delay new phase (Ended) until next frame.
	//			If new phase == Moved and old phase == Stationary => check distance against threshold for Moved/Stationary.
	//
	// After every frame, loop through all active touch slots:
	//			If a delayed phase (Ended) was enabled => update the phase.
	//			If a touch != Ended wasn't updated this frame => set phase = Stationary
	//			If a touch began and ended the this frame, and it resulted in another, currently active, touch having an increased tap count
	//				=> clear those extra touches, and compact the finger id

	// NB: for now we do use passed position as both raw position and position
	// on ios, where we actually implement raw position, different code is used

	FreeExpiredTouches(m_FrameCount, timestamp);

	TouchImpl*	matchingSlots[kMaxTouchCount];
	const size_t slotsFound = FindByPointerId(matchingSlots, pointerId);

	TouchImpl* touch = NULL;
	int inheritedTapCount = 0;

#if UNITY_WINRT
	// [Metro] Calculate tapCount manually as pointerIds aren't reused.
	if (TouchImpl::IsBegin(newPhase))
		inheritedTapCount += CalculateTapCount(timestamp, pos);
#endif

	for (size_t i = 0; i < slotsFound; ++i)
	{
		TouchImpl* slot = matchingSlots[i];

		bool touchFinished = slot->isFinished() || slot->willBeFinishedNextFrame();
		if (TouchImpl::IsBegin(newPhase))
		{
			if (touchFinished)
			{
				if (slot->isOld(m_FrameCount))
				{
					touch = slot;
				}

				if (slot->isMultitap(timestamp, pos, m_ScreenDPI))
				{
					inheritedTapCount = slot->tapCount;
				}
			}
		}
		else
		{
			if (!touchFinished)
			{
				if (touch)
				{
					if (DEBUGMODE) printf_console("Stale/stuck touch released.");
					ExpireOld(*touch);
				}
				touch = slot;
			}
		}
	}

	if (!touch)
	{
		if (!TouchImpl::IsBegin(newPhase))
		{
			if (DEBUGMODE) printf_console("Dropping touch event part of canceled gesture.");
			return;
		}
		if (!(touch = AllocateNew()))
			return;
	}

	if (TouchImpl::IsBegin (newPhase))
	{
		touch->tapCount = inheritedTapCount;
	#if DEBUG_TOUCH_EMU
		printf_console("Slot before initialized:");
		touch->dump(m_TouchSlots);
	#endif

		touch->init( pointerId, pos, newPhase, timestamp, currFrame );
		touch->setTapCount( timestamp, pos, m_ScreenDPI );
		touch->id = CompactFingerID(touch->id);

	#if DEBUG_TOUCH_EMU
		printf_console("Slot initialized:");
		touch->dump(m_TouchSlots);
	#endif
		return;
	}
	else if (TouchImpl::IsEnd (newPhase))
	{
		// if touch began this frame, we will delay end phase for one frame
		if (touch->frameBegan == currFrame)
			touch->endPhaseInQueue = newPhase;
		else
			touch->phase = newPhase;

		// Android only sends CANCELED on the first pointer in a gesture - kill all other active touches when this happens.
		if (newPhase == kTouchCanceled)
		{
			for (size_t i = 0; i < kMaxTouchCount; ++i)
			{
				TouchImpl* slot = &m_TouchSlots[i];
				if (slot->isEmpty() || slot->isFinished() || slot->willBeFinishedNextFrame())
					continue;
				slot->endPhaseInQueue = newPhase;
		#if DEBUG_TOUCH_EMU
				m_TouchSlots[i].dump(m_TouchSlots);
		#endif
			}
		}
	}
	else if (newPhase == kTouchMoved
	         && touch->phase == kTouchStationary)
	{
		// old event is STATIONARY, the new one is MOVE. Android does not
		// report STATIONARY Events, so if MOVE's deltaPos is not big enough,
		// let's keep it STATIONARY. Promote to MOVE otherwise.
		static const float deltaPosTolerance = 0.5f;
		if (Magnitude (touch->pos - pos) >= deltaPosTolerance)
			touch->phase = newPhase;
	}

	touch->setDeltaPos (pos);
	touch->pos = pos;

	touch->setDeltaTime (timestamp);
	touch->timestamp = timestamp;
	touch->frameToReport = currFrame;

#if DEBUG_TOUCH_EMU
	printf_console("Slot updated:");
	touch->dump(m_TouchSlots);
#endif

}

size_t TouchPhaseEmulation::FindByPointerId(TouchImpl* matchingSlots[kMaxTouchCount], size_t pointerId)
{
#if DEBUG_TOUCH_EMU
	printf_console("%s", __FUNCTION__);
#endif
	size_t slotsFound = 0;
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		if (m_TouchSlots[i].pointerId != pointerId)
			continue;
#if DEBUG_TOUCH_EMU
		m_TouchSlots[i].dump(m_TouchSlots);
#endif
		matchingSlots[slotsFound++] = &m_TouchSlots[i];
	}
	return slotsFound;
}

TouchImpl* TouchPhaseEmulation::AllocateNew()
{
#if DEBUG_TOUCH_EMU
	printf_console("%s", __FUNCTION__);
#endif
	// allocate virtual fingerId
	int fingerId = 0;
	const int maxFingerId = sizeof(m_AllocatedFingerIDs) * 8;
	for (; fingerId < maxFingerId; ++fingerId)
	{
		UInt32 bitField = (1 << fingerId);
		if (m_AllocatedFingerIDs & bitField)
			continue;
		m_AllocatedFingerIDs |= bitField;
		break;
	}
	if (fingerId >= maxFingerId)
	{
		Assert (!"Out of virtual finger IDs!");
		return NULL;
	}

	// find empty slot for touch
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		TouchImpl& t = m_TouchSlots[i];

		if (!t.isEmpty())
			continue;

		t.id				= fingerId;
		t.deltaPos			= Vector2f(0, 0);
		t.deltaTime			= 0.0f;
		t.endPhaseInQueue	= 0;

		return &t;
	}

	Assert (!"Out of free touches!");
	return NULL;
}

void TouchPhaseEmulation::ExpireOld(TouchImpl& touch)
{
#if DEBUG_TOUCH_EMU
	printf_console("%s", __FUNCTION__);
#endif

	if (touch.isEmpty())
	{
		ErrorString("Trying to expire empty touch slot!");
		return;
	}

	// deallocate virtual fingerId
	UInt32 bitField = (1 << touch.id);
	Assert((m_AllocatedFingerIDs & bitField) && "Touch with stale finger ID killed!");
	m_AllocatedFingerIDs &= ~bitField;

	Assert(!touch.endPhaseInQueue && "Delayed touch killed prematurely!");
	touch.clear();
}

int TouchPhaseEmulation::CompactFingerID(int id)
{
	int fingerId = 0;
	const int maxFingerId = sizeof(m_AllocatedFingerIDs) * 8;
	for (; fingerId < maxFingerId; ++fingerId)
	{
		UInt32 bitField = (1 << fingerId);
		if (m_AllocatedFingerIDs & bitField)
			continue;

		if (id < fingerId)
			return id;

		m_AllocatedFingerIDs |= bitField;
		bitField = (1 << id);
		Assert((m_AllocatedFingerIDs & bitField) && "Touch with stale finger ID killed!");
		m_AllocatedFingerIDs &= ~bitField;
		id = fingerId;
		break;
	}
	return id;
}

void TouchPhaseEmulation::FreeExpiredTouches (size_t eventFrame, long long timestamp)
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		TouchImpl& touch = m_TouchSlots[i];

		if (touch.isEmpty())
			continue;

		long long age = timestamp - touch.timestamp;
		if (touch.isOld(eventFrame) && touch.isFinished() && age > touchTimeout)
		{
			ExpireOld(touch);
		}
	}
}

void TouchPhaseEmulation::DiscardRedundantTouches()
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		TouchImpl* t0 = &m_TouchSlots[i];
		TouchImpl* t1 = 0;

		if (t0->isEmpty())
			continue;

		// find Down/Up touches recorded within a single frame
		bool downUpTouch =
			t0->frameBegan == m_FrameCount &&
			t0->frameToReport == m_FrameCount &&
			t0->willBeFinishedNextFrame() &&
			!t0->isFinished();

		if (!downUpTouch)
			continue;

	#if DEBUG_TOUCH_EMU
		printf_console("Found new touch set to expire next frame");
		t0->dump(m_TouchSlots);
	#endif

		bool redundant = false;

		// compare the multitap info
		for (size_t j = 0; j < kMaxTouchCount; ++j)
		{
			t1 = &m_TouchSlots[j];

			if (t1->isEmpty() || i == j)
				continue;

			bool multitapTouch =
				t1->frameBegan == m_FrameCount &&
				t1->frameToReport == m_FrameCount &&
				t1->pointerId == t0->pointerId &&
				t1->tapCount > t0->tapCount &&
				t0->isMultitap(t1->timestamp, t1->pos, m_ScreenDPI) &&
				!t1->isFinished();

			if (!multitapTouch)
				continue;

		#if DEBUG_TOUCH_EMU
			printf_console("Found new touch, with tapCount that matches the one found earlier");
			t1->dump(m_TouchSlots);
		#endif
			// found a match
			redundant = true;
			break;
		}

		if (redundant)
		{
			t0->endPhaseInQueue = 0;		// this touch is officially gone anyway
			ExpireOld(*t0);
			t1->id = CompactFingerID(t1->id);
		#if DEBUG_TOUCH_EMU
			printf_console("New touch, with compacted finger id");
			t1->dump(m_TouchSlots);
		#endif
		}
		else
		{
			t0->id = CompactFingerID(t0->id);
		#if DEBUG_TOUCH_EMU
			printf_console("Refresh touch, with compacted finger id");
			t0->dump(m_TouchSlots);
		#endif
		}
	}
}

void TouchPhaseEmulation::UpdateActiveTouches()
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		TouchImpl& touch = m_TouchSlots[i];

		if (touch.isEmpty())
			continue;

		// Skip Expired Touches
		if (touch.isFinished())
		{
			continue;
		}

		// End Delayed Touches
		if (touch.willBeFinishedNextFrame ())
		{
			touch.deltaPos			= Vector2f (0, 0);
			touch.phase				= touch.endPhaseInQueue;
			touch.endPhaseInQueue	= 0;
			touch.frameToReport		= m_FrameCount;
			continue;
		}

		// Default Stationary Touches
		touch.phase			= kTouchStationary;
		touch.deltaPos		= Vector2f (0, 0);
		touch.frameToReport	= m_FrameCount;
	}

}

size_t TouchPhaseEmulation::GetTouchCount ()
{
	size_t count = 0;

	// TODO: on first call to GetTouchCount() per frame, call PackTouchIds() to
	// compact virtual touch IDs to stand out less

	for (size_t i = 0; i < kMaxTouchCount; ++i)
		if ( m_TouchSlots[i].isNow(m_FrameCount) &&
			!m_TouchSlots[i].isEmpty() )
			++count;

	return count;
}

size_t TouchPhaseEmulation::GetActiveTouchCount ()
{
	size_t count = 0;

	for (size_t i = 0; i < kMaxTouchCount; ++i)
		if (!m_TouchSlots[i].isEmpty() && !m_TouchSlots[i].isFinished())
			++count;

	return count;
}

// @param index  Zero-based index of events that are to be reported this
//               frame. Since not all of the events in the container need to
//               be reported this frame, it's used to skip already reported
//               ones.
bool TouchPhaseEmulation::GetTouch (size_t index, Touch& touch)
{
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		if ( m_TouchSlots[i].isNow(m_FrameCount) &&
			!m_TouchSlots[i].isEmpty() &&
			index-- == 0 )
		{
			touch = m_TouchSlots[i];
			return true;
		}
	}

	return false;
}

bool TouchPhaseEmulation::IsMultiTouchEnabled ()
{
	if (m_IsSingleTouchDevice)
		return false;

	return m_IsMultiTouchEnabled;
}

void TouchPhaseEmulation::SetMultiTouchEnabled (bool enabled)
{
	if (m_IsSingleTouchDevice)
		return;

	m_IsMultiTouchEnabled = enabled;
}

int TouchPhaseEmulation::CalculateTapCount( long long timestamp, Vector2f const &pos ) const
{
	int result = 0;
	for (size_t i = 0; i < kMaxTouchCount; ++i)
	{
		TouchImpl& touch = m_TouchSlots[i];

		if (touch.isEmpty())
			continue;

		if (touch.isMultitap(timestamp, pos, m_ScreenDPI))
			result += touch.tapCount;
	}

	return result;
}

#if DEBUG_TOUCH_EMU
void TouchPhaseEmulation::DumpAll (bool verbose)
{
	for (int i = 0; i < kMaxTouchCount; ++i)
		if (!m_TouchSlots[i].isEmpty() && !m_TouchSlots[i].isOld(m_FrameCount) || verbose)
			m_TouchSlots[i].dump( m_TouchSlots );
}
#endif
