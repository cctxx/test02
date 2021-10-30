#pragma once

#include <vector>

enum ScreenOrientation
{
	kScreenOrientationUnknown,

	kPortrait,
	kPortraitUpsideDown,
	kLandscapeLeft,
	kLandscapeRight,

	kAutoRotation,

	kScreenOrientationCount
};

inline ScreenOrientation PlayerSettingsToScreenOrientation(int defaultScreenOrientation)
{
	switch (defaultScreenOrientation) {
		case 0 :
			return kPortrait;
		case 1 :
			return kPortraitUpsideDown;
		case 2 :
			return kLandscapeRight;
		case 3 :
			return kLandscapeLeft;
		case 4 :
			return kAutoRotation;
		default:
			return kScreenOrientationUnknown;
	}
}

inline int ScreenOrientationToPlayerSettings(ScreenOrientation screenOrientation)
{
	switch (screenOrientation) {
		case kPortrait :
			return 0;
		case kPortraitUpsideDown :
			return 1;
		case kLandscapeRight :
			return 2;
		case kLandscapeLeft :
			return 3;
		case kAutoRotation :
			return 4;
		default:
			return 5;
	}
}


enum SleepTimeout
{
	kNeverSleep = -1,
	kSystemSetting = -2,
};

enum EnabledOrientation
{
	kAutorotateToPortrait = 1,
	kAutorotateToPortraitUpsideDown = 2,
	kAutorotateToLandscapeLeft = 4,
	kAutorotateToLandscapeRight = 8
};

#define kResolutionWidth "Screenmanager Resolution Width"
#define kResolutionHeight "Screenmanager Resolution Height"
#define kIsFullScreen "Screenmanager Is Fullscreen mode"
#define kGraphicsQuality "UnityGraphicsQuality"

class ScreenManager
{
public:
	ScreenManager ();

	void RequestResolution (int width, int height, bool fullscreen, int preferredRefreshRate);
	void RequestSetFullscreen (bool fullscreen);
	bool HasFullscreenRequested () const;

	virtual void SetRequestedResolution ();
	void SetIsFullScreenImmediate (bool fullscreen);
	virtual bool SetResolutionImmediate (int /*width*/, int /*height*/, bool /*fullscreen*/, int /*preferredRefreshRate*/) { return false; }

	struct Resolution
	{
		// Keep in sync with .NET Resolution struct!
		int width;
		int height;
		int refreshRate;

		friend bool operator < (const Resolution& lhs, const Resolution& rhs)
		{
			if( lhs.width != rhs.width )
				return lhs.width < rhs.width;
			return lhs.height < rhs.height;
		}

		/**
		 *	Returns true if this resolution is >= given width/height.
		 *  Used to filter out resolutions that are too big for windowed modes.
		 */
		bool IsTooBigFor( int w, int h ) const
		{
			return width >= w || height >= h;
		}

		bool IsRotated() const { return height > width; }

		Resolution ()
		{
			width = 0;
			height = 0;
			refreshRate = 0;
		}
	};

	struct ResolutionRequest
	{
		ResolutionRequest() { Reset(); }

		int width;
		int height;
		int fullScreen;
		int refreshRate;

		bool IsFullScreen() const { return fullScreen == 1; }

		void Reset()
		{
			width = -1;
			height = -1;
			fullScreen = -1;
			refreshRate = -1;
		}
	};

	typedef std::vector<Resolution> Resolutions;
	virtual Resolutions GetResolutions (int /*preferredRefreshRate*/ = 0, bool /*clampToDesktopRes*/ = false) { return Resolutions(); }
	int FindClosestResolution (const ScreenManager::Resolutions& resolutions, int width, int height) const;

	virtual Resolution GetCurrentResolution() const;

	virtual bool GetShowCursor () const { return true; }
	virtual void SetShowCursor (bool /*show*/) { }

	virtual bool GetLockCursor () const { return false; }
	virtual void SetLockCursor (bool /*lock*/) { }

	virtual bool GetAllowLayeredRendering () const { return true; }
	virtual void SetAllowLayeredRendering (bool /*allow*/) { }

	virtual int GetScreenTimeout () const { return kNeverSleep; }
	virtual void SetScreenTimeout (int /*value*/) { }

	bool GetAllowCursorHide() const { return m_AllowCursorHide; }
	void SetAllowCursorHide (bool allowHide);

	bool GetAllowCursorLock() const { return m_AllowCursorLock; }
	void SetAllowCursorLock (bool allowLock);

	// Named *IsFocus to avoid confusion with windows API
	virtual bool GetIsFocused() const { return true; }
	virtual void SetIsFocused (bool /*focus*/) { }

	virtual bool GetCursorInsideWindow () const { return m_CursorInWindow; }
	virtual void SetCursorInsideWindow (bool insideWindow);

	// Do these need to be virtual?
	virtual int GetWidth () const   { return m_Width; }
	virtual int GetHeight () const  { return m_Height; }

	int GetWidthAsRequested () const   { return m_ResolutionRequest.width == -1 ? GetWidth () : m_ResolutionRequest.width; }
	int GetHeightAsRequested () const  { return m_ResolutionRequest.height == -1 ? GetHeight () : m_ResolutionRequest.height; }
	bool GetFullScreenAsRequested () const  { return m_ResolutionRequest.fullScreen == -1 ? IsFullScreen () : m_ResolutionRequest.IsFullScreen (); }
	int GetRefreshRateAsRequested () const  { return m_ResolutionRequest.refreshRate == -1 ? GetCurrentResolution ().refreshRate : m_ResolutionRequest.refreshRate; }

	virtual float GetDPI () const  { return 0.f; }
	virtual bool IsFullScreen () const         { return m_IsFullscreen; }

	void RequestOrientation(ScreenOrientation value)	{ m_RequestedOrientation = value; }
	ScreenOrientation GetRequestedOrientation() const	{ return m_RequestedOrientation; }

	virtual ScreenOrientation GetScreenOrientation() const { return m_ScreenOrientation; };
	virtual void SetScreenOrientation (ScreenOrientation value) { m_ScreenOrientation = value; }

	typedef void DidSwitchResolutions ();
	void RegisterDidSwitchResolutions (DidSwitchResolutions* resolution);

	virtual void SetupScreenManagerEditor(float w, float h);

	void SetIsOrientationEnabled(EnabledOrientation orientation, bool enabled);
	bool GetIsOrientationEnabled(EnabledOrientation orientation) const { return m_EnabledOrientations & orientation; }

	// helpers for connecting ScreenManager and PlayerSettings
	void EnableOrientationsFromPlayerSettings();
	void SetConcreteOrientationFromPlayerSettings(int playerSettingsOrient);
	void RequestConcreteOrientationFromPlayerSettings(int playerSettingsOrient);

protected:
	ResolutionRequest m_ResolutionRequest;

	DidSwitchResolutions* m_SwitchResolutionCallback;

	bool m_AllowCursorHide;
	bool m_AllowCursorLock;
	bool m_CursorInWindow;
	bool m_IsFullscreen;

	int m_Width;
	int m_Height;

	unsigned int m_EnabledOrientations;
	ScreenOrientation m_ScreenOrientation;
	ScreenOrientation m_RequestedOrientation;
};

#if UNITY_PEPPER
#include "PlatformDependent/PepperPlugin/ScreenManagerPepper.h"
#elif UNITY_OSX
#include "PlatformDependent/OSX/ScreenManagerOSX.h"
#elif UNITY_WP8
#include "PlatformDependent/WP8Player/ScreenManagerWP8.h"
#elif UNITY_METRO
#include "PlatformDependent/MetroPlayer/ScreenManagerMetro.h"
#elif UNITY_WIN
#include "../../PlatformDependent/WinPlayer/ScreenManagerWin.h"
#elif UNITY_XENON
#include "../../PlatformDependent/Xbox360/Source/ScreenManagerXenon.h"
#elif UNITY_PS3
#include "../../PlatformDependent/PS3Player/ScreenManagerPS3.h"
#elif UNITY_ANDROID
#include "PlatformDependent/AndroidPlayer/ScreenManagerAndroid.h"
#elif UNITY_IPHONE
#include "PlatformDependent/iPhonePlayer/ScreenManagerIPhone.h"
#elif UNITY_LINUX && SUPPORT_X11
#include "PlatformDependent/Linux/ScreenManagerLinux.h"
#elif UNITY_WII
#include "PlatformDependent/Wii/WiiScreenManager.h"
#elif UNITY_FLASH
#include "PlatformDependent/FlashSupport/cpp/FlashScreenManager.h"
#elif UNITY_BB10
#include "PlatformDependent/BB10Player/ScreenManagerBB10.h"
#elif UNITY_TIZEN
#include "PlatformDependent/TizenPlayer/ScreenManagerTizen.h"
#else
#define ScreenManagerPlatform ScreenManager
#endif

void InitScreenManager();
void ReleaseScreenManager();
ScreenManagerPlatform &GetScreenManager();
ScreenManagerPlatform* GetScreenManagerPtr();

