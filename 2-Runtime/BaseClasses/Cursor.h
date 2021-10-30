#pragma once

#include "UnityPrefix.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Math/Vector2.h"

#include <string>

#define PLATFORM_SUPPORTS_HARDWARE_CURSORS (!UNITY_PEPPER && ((UNITY_WIN && !UNITY_WINRT) || UNITY_OSX || UNITY_LINUX || UNITY_FLASH))

enum CursorMode
{
	kAutoHardwareCursor = 0,
	kHardwareCursorOff = 1
};

#if UNITY_OSX
	#ifdef __OBJC__
		@class NSCursor;
	#else
		typedef struct objc_object NSCursor;
	#endif
#endif

namespace Cursors
{

template <typename T>
struct UnityCursor
{
	UnityCursor ()
	{
		hCursor = NULL;
		sCursor = NULL;
	}
	T hCursor;
	PPtr<Texture2D> sCursor;
	Vector2f hotspot;

	typedef T HCursorType;
};

template <typename T>
struct CursorManager
{
	T m_DefaultCursor;
	T m_CurrentCursor;

	bool m_UsingBuiltinDefaultCursor;

	typedef std::map<TextureID, T > CursorCache;
	CursorCache m_CursorCache;

	Texture2D* GetSoftwareCursor ()
	{
		return m_CurrentCursor.sCursor;
	}

	Vector2f GetCursorHotspot ()
	{
		return m_CurrentCursor.hotspot;
	}

	typename T::HCursorType GetHardwareCursor ()
	{
		return m_CurrentCursor.hCursor;
	}

	static CursorManager<T>* s_CursorManager;
	static CursorManager<T>& Instance ()
	{
		if (s_CursorManager == NULL)
		{
			s_CursorManager = new CursorManager<T>();
		}

		return *s_CursorManager;
	}

	static void Cleanup ()
	{
		delete s_CursorManager;
		s_CursorManager = NULL;
	}
};

void SetCursor (Texture2D* texture, Vector2f hotSpot, CursorMode forceHardware);
void RenderSoftwareCursor ();
Texture2D* GetSoftwareCursor ();
Vector2f GetCursorHotspot ();
void InitializeCursors (Texture2D* defaultCursorTexture, Vector2f defaultCursorHotSpot);
void CleanupCursors ();

#if UNITY_WIN
void ResetCursor ();
// returns true if the event is 'handled' WM_SETCURSOR in this case
bool HandleMouseCursor (UINT message, LPARAM lParam);
HCURSOR GetHardwareCursor ();
#endif

#if UNITY_OSX
void ResetCursor ();
NSCursor* GetHardwareCursor ();
#endif
};
