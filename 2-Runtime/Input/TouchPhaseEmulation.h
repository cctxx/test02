#ifndef __UNITY_INPUT_TOUCHPHASEEMULATION_H
#define __UNITY_INPUT_TOUCHPHASEEMULATION_H

#include "Runtime/Math/Vector2.h"
#include "GetInput.h"

class TouchImpl;

class TouchPhaseEmulation
{
public:
			 TouchPhaseEmulation(float screenDPI, bool singleTouchDevice);
	virtual	~TouchPhaseEmulation();

public:

	void	InitTouches();
	void	PreprocessTouches();
	void	PostprocessTouches();

	enum TouchPhase
	{
		kTouchBegan			= 0,
		kTouchMoved			= 1,
		kTouchStationary	= 2,
		kTouchEnded			= 3,
		kTouchCanceled		= 4
	};

	void	AddTouchEvent (int pointerId, float x, float y, TouchPhase newPhase, long long timestamp);
	size_t	GetTouchCount();
	size_t	GetActiveTouchCount();
	bool	GetTouch(size_t index, Touch& touch);

	bool	IsMultiTouchEnabled ();
	void	SetMultiTouchEnabled (bool enabled);

private:

	void	DispatchTouchEvent (size_t pointerId, Vector2f pos, TouchPhase newPhase, long long timestamp, size_t currFrame);
	bool	IsExistingTouch( int pointerId );

	enum { kMaxTouchCount = 32 };

	size_t		FindByPointerId(TouchImpl* matchingSlots[kMaxTouchCount], size_t pointerId);
	TouchImpl*	AllocateNew();
	void		ExpireOld(TouchImpl& touch);
	int			CompactFingerID(int id);
	void		FreeExpiredTouches (size_t eventFrame, long long timestamp);

	void	DiscardRedundantTouches();
	void	UpdateActiveTouches();
	int		CalculateTapCount( long long timestamp, Vector2f const &pos ) const;

#if DEBUG_TOUCH_EMU
	void DumpAll (bool verbose=false);
#endif

	TouchImpl*	m_TouchSlots; //[kMaxTouchCount];
	UInt32		m_AllocatedFingerIDs;		// holds one bit per finger
	size_t 		m_FrameCount;
	const float	m_ScreenDPI;
	bool 		m_IsMultiTouchEnabled;
	const bool	m_IsSingleTouchDevice;

};

#endif // __UNITY_INPUT_TOUCHPHASEEMULATION_H
