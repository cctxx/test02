#include "UnityPrefix.h"
#include "ScreenManager.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/GfxDeviceSetup.h"

static ScreenManagerPlatform* gScreenManager = NULL;

#if UNITY_IPHONE
	extern "C" void NotifyAutoOrientationChange();
#endif


void InitScreenManager()
{
	Assert(gScreenManager == NULL);
	gScreenManager = new ScreenManagerPlatform();
}

void ReleaseScreenManager()
{
	Assert(gScreenManager != NULL);
	delete gScreenManager;
	gScreenManager = NULL;
}

ScreenManagerPlatform &GetScreenManager()
{
	Assert(gScreenManager != NULL);
	return *gScreenManager;
}

ScreenManagerPlatform* GetScreenManagerPtr()
{
	return gScreenManager;
}

ScreenManager::ScreenManager ()
{
	m_Width = 0;
	m_Height = 0;
	m_SwitchResolutionCallback = NULL;
	m_CursorInWindow = false;
	m_IsFullscreen = false;
	m_AllowCursorHide = true;
	m_AllowCursorLock = true;

	m_ScreenOrientation    = kPortrait;
	m_RequestedOrientation = kScreenOrientationUnknown;

	// Do not allow to autorotate by default.
	m_EnabledOrientations = 0;
}

void ScreenManager::RequestResolution (int width, int height, bool fullscreen, int preferredRefreshRate)
{
	m_ResolutionRequest.width = width;
	m_ResolutionRequest.height = height;
	m_ResolutionRequest.fullScreen = fullscreen ? 1 : 0;
	m_ResolutionRequest.refreshRate = preferredRefreshRate;
}

void ScreenManager::RequestSetFullscreen (bool fullscreen)
{
	m_ResolutionRequest.fullScreen = fullscreen ? 1 : 0;
}

bool ScreenManager::HasFullscreenRequested () const
{
	return m_ResolutionRequest.fullScreen == 1;
}

void ScreenManager::SetCursorInsideWindow (bool insideWindow)
{
	m_CursorInWindow = insideWindow;
	SetShowCursor(GetShowCursor());
}

int ScreenManager::FindClosestResolution (const ScreenManager::Resolutions& resolutions, int width, int height) const
{
	if (resolutions.empty ())
		return -1;

	int maxDistance = std::numeric_limits<int>::max ();
	int index = 0;
	for (int i=0;i<resolutions.size ();i++)
	{
		int curWidth = resolutions[i].width;
		int curHeight = resolutions[i].height;
		int distance = Abs (width - curWidth) + Abs (height - curHeight);
		if (distance < maxDistance)
		{
			index = i;
			maxDistance = distance;
		}
	}
	return index;
}

void ScreenManager::RegisterDidSwitchResolutions (DidSwitchResolutions* resolution)
{
	m_SwitchResolutionCallback = resolution;
}

void ScreenManager::SetRequestedResolution ()
{
	if (m_ResolutionRequest.width != -1 && m_ResolutionRequest.height != -1)
	{
		SetResolutionImmediate (m_ResolutionRequest.width, m_ResolutionRequest.height, m_ResolutionRequest.IsFullScreen(), m_ResolutionRequest.refreshRate);
		m_ResolutionRequest.Reset();
	}

	if (m_ResolutionRequest.fullScreen != -1)
	{
		SetIsFullScreenImmediate (m_ResolutionRequest.fullScreen ? true : false);
		m_ResolutionRequest.fullScreen = -1;
	}
}

void ScreenManager::SetIsFullScreenImmediate (bool fullscreen)
{
	if (fullscreen != IsFullScreen())
		SetResolutionImmediate (GetWidth (), GetHeight (), fullscreen, 0);
}

ScreenManager::Resolution ScreenManager::GetCurrentResolution() const
{
	Resolution res;
	res.width = GetWidth();
	res.height = GetHeight();
	res.refreshRate = 0;
	return res; //@TODO
}

void ScreenManager::SetupScreenManagerEditor( float w, float h )
{
	m_Width = RoundfToIntPos(w);
	m_Height = RoundfToIntPos(h);
}


void ScreenManager::SetAllowCursorHide (bool allowHide)
{
	m_AllowCursorHide = allowHide;
	if (!m_AllowCursorHide)
		SetShowCursor(true);
}

void ScreenManager::SetAllowCursorLock (bool allowLock)
{
	m_AllowCursorLock = allowLock;
	if (!m_AllowCursorLock)
		SetLockCursor(false);
}


void ScreenManager::SetIsOrientationEnabled(EnabledOrientation orientation, bool enabled)
{
#if UNITY_IPHONE
	NotifyAutoOrientationChange();
#endif

#if UNITY_WP8
	// upside down portrait is not available on wp8
	if (orientation == EnabledOrientation::kAutorotateToPortraitUpsideDown)
		enabled = false;
#endif

	if (enabled)
		m_EnabledOrientations |= orientation;
	else
		m_EnabledOrientations &= ~orientation;
}


#include "Runtime/Misc/PlayerSettings.h"

void ScreenManager::EnableOrientationsFromPlayerSettings()
{
	SetIsOrientationEnabled(kAutorotateToPortrait, GetPlayerSettings().GetAutoRotationAllowed(0));
	SetIsOrientationEnabled(kAutorotateToPortraitUpsideDown, GetPlayerSettings().GetAutoRotationAllowed(1));
	SetIsOrientationEnabled(kAutorotateToLandscapeRight, GetPlayerSettings().GetAutoRotationAllowed(2));
	SetIsOrientationEnabled(kAutorotateToLandscapeLeft, GetPlayerSettings().GetAutoRotationAllowed(3));
}

#define INIT_ORIENT_FROM_PLAYER_SETTINGS_IMPL(name, func)				\
void ScreenManager::name(int playerSettingsOrient)						\
{																		\
	switch(playerSettingsOrient)										\
	{																	\
		case 0: func(kPortrait);			break;						\
		case 1: func(kPortraitUpsideDown);	break;						\
		case 2: func(kLandscapeRight);		break;						\
		case 3: func(kLandscapeLeft);		break;						\
																		\
		default: Assert(false && #name "do not accept autorotation.");	\
	}																	\
}																		\

INIT_ORIENT_FROM_PLAYER_SETTINGS_IMPL(SetConcreteOrientationFromPlayerSettings, SetScreenOrientation);
INIT_ORIENT_FROM_PLAYER_SETTINGS_IMPL(RequestConcreteOrientationFromPlayerSettings, RequestOrientation);
