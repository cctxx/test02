#include "UnityPrefix.h"
#include "SimulateInputEvents.h"
#include "GetInput.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "InputManager.h"
#include "Runtime/IMGUI/GUIManager.h"
#include <math.h>

#if UNITY_EDITOR && UNITY_OSX
#include "Editor/Src/RemoteInput/iPhoneRemoteImpl.h"
#endif

#if UNITY_EDITOR
#include "Editor/Src/RemoteInput/AndroidRemote.h"
#endif

#ifndef MAXFLOAT
	#define	MAXFLOAT	((float)3.40282346638528860e+38)
#endif

void CaptureEventMousePosition (InputEvent& e)
{
	e.Init();

	Vector2f p = GetInputManager().GetMousePosition();

#if ENABLE_NEW_EVENT_SYSTEM
	e.touch.pos = p;
	e.touch.pos.y = GetScreenManager().GetHeight() - p.y;
	e.touch.deltaPos = GetInputManager().GetMouseDelta();
#else
	e.mousePosition = p;
	e.mousePosition.y = GetScreenManager().GetHeight() - e.mousePosition.y;
	Vector3f d = GetInputManager().GetMouseDelta();
	e.delta = Vector2f(d.x, d.y);
#endif
	e.pressure = 1.0f;

    e.clickCount = 1;

    size_t touchCount = GetActiveTouchCount();

	for (int i = 0; i < touchCount; ++i)
	{
#if ENABLE_NEW_EVENT_SYSTEM
		Touch* touch = GetTouch(i);

		if (touch != NULL)
        {
			if (touch->tapCount > e.clickCount)
            {
                e.clickCount = touch->tapCount;
            }
        }
#else
		Touch touch;

		if (GetTouch(i, touch))
		{
			if (touch.tapCount > e.clickCount)
			{
				e.clickCount = touch.tapCount;
			}
		}
#endif
	}
}

// send event on button down/up
void GenerateAndSendInputDownUpEvent( const InputEvent::MouseButton button, const bool isDown )
{
	InputEvent ie;
	CaptureEventMousePosition (ie);
	ie.button = button;
	ie.type = isDown ? InputEvent::kMouseDown : InputEvent::kMouseUp;
#if UNITY_METRO
	ie.touchType = InputEvent::kFingerTouch;
#endif
	GetGUIManager().QueueEvent (ie);
	// If the touch has ended we need to "disable" the mouse hover state, by moving the mouse pointer "away"
	if (!isDown)
	{
		ie.Init();
		ie.type = InputEvent::kMouseUp;
#if ENABLE_NEW_EVENT_SYSTEM
		ie.touch.pos = Vector2f(MAXFLOAT,MAXFLOAT);
#else
		ie.mousePosition = Vector2f(MAXFLOAT,MAXFLOAT);
#endif
		GetGUIManager().QueueEvent (ie);
	}
}

void RestoreMouseState( const Vector2f& mousePosition )
{
	enum { MaxSimulatedMouseButtons = 3 };
	GetInputManager().SetMousePosition(mousePosition);
	if (GetActiveTouchCount() > 0)
	{
		for (int i = 0; i < MaxSimulatedMouseButtons; ++i)
			GetInputManager().SetMouseButton(i, false);
		GetInputManager().InputEndFrame();
	}
}

void SimulateMouseInput()
{
	enum { MaxSimulatedMouseButtons = 3 };
	static size_t prevTouchPointCount = 0;

#if UNITY_WINRT
	Vector2f originalPos = GetInputManager().GetMousePosition();
#endif

	for (int i = 0; i < MaxSimulatedMouseButtons; ++i)
	{
		if (i < GetActiveTouchCount())
			GetInputManager().SetMouseButton(i, true);
		else if (i < prevTouchPointCount)
			GetInputManager().SetMouseButton(i, false);
	}

	prevTouchPointCount = GetActiveTouchCount();

	size_t touchCount = GetTouchCount();
	Vector2f pos(0.0f, 0.0f);
	static Vector2f prevPos(0.0f, 0.0f);

	for (int i = 0; i < GetTouchCount(); ++i)
	{
#if ENABLE_NEW_EVENT_SYSTEM
		Touch* touch = GetTouch(i);
		if (touch != NULL)
			pos += touch->pos;
#else
		Touch touch;
		if (GetTouch(i, touch))
			pos += touch.pos;
#endif
	}

	if (touchCount > 0)
	{
		float invCount = 1.0f / (float)touchCount;
		pos.x *= invCount;
		pos.y *= invCount;

		GetInputManager().SetMousePosition(pos);
#if ENABLE_NEW_EVENT_SYSTEM
		GetInputManager().SetMouseDelta(Vector2f(pos.x - prevPos.x, pos.y - prevPos.y));
#else
		GetInputManager().SetMouseDelta(Vector3f(pos.x - prevPos.x,
		                                         pos.y - prevPos.y, 0.0f));
#endif
		prevPos = pos;
	}
	
#if UNITY_IPHONE || UNITY_ANDROID || UNITY_WINRT || UNITY_BB10 || UNITY_TIZEN
	SimulateInputEvents ();

#elif UNITY_EDITOR
	if (
#if UNITY_OSX
		iPhoneHasRemoteConnected () ||
#endif
		AndroidHasRemoteConnected())
	{
		SimulateInputEvents();
	}
#endif

#if UNITY_WINRT
	if (!GetInputManager().GetSimulateMouseWithTouches())
		RestoreMouseState(originalPos);
#endif
}

void SimulateInputEvents()
{
	InputEvent ie;

	static bool lastMouseB0 = false;
	static bool lastMouseB1 = false;

	if (SqrMagnitude(GetInputManager().GetMouseDelta()) > 1e-6)
	{
		CaptureEventMousePosition (ie);
		ie.type = InputEvent::kMouseMove;
		ie.button = 0;
#if UNITY_METRO
		ie.touchType = InputEvent::kFingerTouch;
#endif

		if (GetInputManager().GetMouseButton(0) && lastMouseB0)
		{
			ie.type = InputEvent::kMouseDrag;
			ie.button |= InputEvent::kLeftButton;
		}
		if (GetInputManager().GetMouseButton(1) && lastMouseB1)
		{
			ie.type = InputEvent::kMouseDrag;
			ie.button |= InputEvent::kRightButton;
		}

		GetGUIManager().QueueEvent (ie);
	}

	bool buttonDown = GetInputManager().GetMouseButton(0);
	if (buttonDown != lastMouseB0)
	{
		GenerateAndSendInputDownUpEvent( InputEvent::kLeftButton, buttonDown );
		lastMouseB0 = buttonDown;
	}

	buttonDown = GetInputManager().GetMouseButton(1);
	if (buttonDown != lastMouseB1)
	{
		GenerateAndSendInputDownUpEvent( InputEvent::kRightButton, buttonDown );
		lastMouseB1 = buttonDown;
	}
}

