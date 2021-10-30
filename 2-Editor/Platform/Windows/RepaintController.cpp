#include "UnityPrefix.h"
#include "Editor/Platform/Interface/RepaintController.h"
#include <set>
#include "Runtime/Profiler/ProfilerImpl.h"
#include "Runtime/Profiler/ProfilerHistory.h"
#include "Runtime/Graphics/DrawSplashScreenAndWatermarks.h"
#include "Editor/Src/RemoteInput/AndroidRemote.h"
#include "Editor/Src/RemoteInput/GenericRemote.h"
#include "Runtime/BaseClasses/Cursor.h"


class Renderable;
Renderable& GetGameViewGizmoRenderable(); // GizmoManager.cpp


static std::set<HWND>	s_DrawViews;

void AddSceneRepaintView( HWND window )
{
	s_DrawViews.insert( window );
}

void RemoveSceneRepaintView (HWND window)
{
	s_DrawViews.erase( window );
}



void RepaintAllSceneRepaintViews()
{
	for( std::set<HWND>::iterator i = s_DrawViews.begin(); i != s_DrawViews.end(); ++i )
	{
		SetCurrentView( *i ); // Tell GizmoUtils that Handles.Repaint should repaint _this_ view
		InvalidateRect( *i, NULL, TRUE );
	}
	SetCurrentView (NULL);
}

static HWND s_CurrentView = NULL;


void RepaintCurrentView () {
	AssertIf( !s_CurrentView );
	InvalidateRect( s_CurrentView, NULL, TRUE );
}

void SetCurrentView (void *currentView)
{
	s_CurrentView = (HWND)currentView;
}

void RepaintView (void * view) {
	HWND wnd = (HWND)view;
	InvalidateRect( wnd, NULL, TRUE );
}



// ----------------------------------------------------------------------
// GameView functionality

#include "Runtime/Math/Rect.h"
#include "Runtime/Misc/InputEvent.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Editor/Src/Application.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/Input/GetInput.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Misc/Player.h"
#include "Runtime/Misc/CaptureScreenshot.h"


HWND GetMainEditorWindow();


void* GetScreenParamsFromGameView(bool updateScreenManager, bool setActiveView, bool* outHasFocus, Rectf* outGUIRect, Rectf* outCameraRect)
{
	GUIView* gameview;
	bool hasFocus;
	GetGameViewAndRectAndFocus(outGUIRect, &gameview, outHasFocus);

	if (gameview)
	{
		RECT rc = {0,0,0,0};
		GetClientRect(gameview->GetWindowHandle(), &rc);
		float viewHeight = rc.bottom;
		*outCameraRect = Rectf (outGUIRect->x, viewHeight - outGUIRect->GetBottom(), outGUIRect->Width(), outGUIRect->Height());
	}
	else
	{
		*outCameraRect = *outGUIRect;
	}

	if (setActiveView && gameview && gameview->GetGfxWindow())
	{
		Assert (updateScreenManager); // if setActiveView is set, should always update screen manager 
		gameview->GetGfxWindow()->SetAsActiveWindow();
	}

	if (updateScreenManager)
	{
		GetRenderManager().SetWindowRect (*outCameraRect);
		GetScreenManager().SetupScreenManagerEditor (outCameraRect->width, outCameraRect->height);
	}

	return gameview;
}


void UpdateScreenManagerAndInput()
{
	Rectf guiRect, cameraRect;
	bool hasFocus;
	GUIView* gameview = (GUIView*)GetScreenParamsFromGameView(true, true, &hasFocus, &guiRect, &cameraRect);

	if (hasFocus && IsWorldPlaying())
	{
		HWND gameWindow = GetMainEditorWindow();
		if (gameview)
			gameWindow = gameview->GetWindowHandle();
		SetEditorMouseOffset (-guiRect.x, -guiRect.y, gameWindow);
		// TODO: !GetPlayerPause() ?
		InputProcess();
	}
}


void ResetEditorMouseOffset ()
{
	SetEditorMouseOffset(0, 0, GetMainEditorWindow());
}



void RenderGameViewCameras(const Rectf& guiRect, bool gizmos, bool gui)
{
	float screenHeight = GetRenderManager().GetWindowRect().Height();
	Rectf cameraRect = Rectf (guiRect.x, screenHeight - guiRect.GetBottom(), guiRect.Width(), guiRect.Height());
	Rectf oldRenderRect = GetRenderManager().GetWindowRect();
	int oldEditorScreenWidth =GetScreenManager().GetWidth ();
	int oldEditorScreenHeight =GetScreenManager().GetHeight ();
	
	// Since update & offscreen rendering is not done for each repaint (only when something changes),
	// we store gathered stats for that, and add actual repaint stats on top.
	GfxDevice& device = GetGfxDevice();
	device.RestoreDrawStats();
	device.BeginFrameStats();
	#if ENABLE_PROFILER
	bool inPlayMode = IsWorldPlaying() && !GetApplication().IsPaused();
	if (inPlayMode)
		UnityProfiler::Get().StartProfilingMode(kProfilerGame);
	#endif

	// Setup Rendering Rects
	ScreenManager& screen = GetScreenManager();
	screen.SetupScreenManagerEditor( cameraRect.width, cameraRect.height );
	GetRenderManager().SetWindowRect (cameraRect);

	if( gizmos )
		GetRenderManager().AddCameraRenderable( &GetGameViewGizmoRenderable(), kQueueIndexMax-1 );

	//	[self UpdateGlobalWindowCenter]; @TODO> REQUIRED

	device.GetFrameStats().SetScreenParams( cameraRect.Width(), cameraRect.Height(), 4, 4, 4, device.GetCurrentTargetAA() );

	// Perform house keeping
	GetApplication().PerformDrawHouseKeeping();

	// offscreen cameras are drawn separately, before any views are rendered
	GetRenderManager().RenderCameras();

	// Repaint all GUI elements...
	GUIView* guiView = GUIView::GetCurrent();
	HWND currentWindow = guiView->GetWindowHandle();
	SetEditorMouseOffset( -guiRect.x, -guiRect.y, currentWindow );

	if (gui)
	{
		InputEvent e = InputEvent::RepaintEvent (currentWindow);
		GetGUIManager().DoGUIEvent(e, false);
	}

	

	if( gizmos )
		GetRenderManager().RemoveCameraRenderable( &GetGameViewGizmoRenderable() );

	Cursors::RenderSoftwareCursor();

	PlayerSendFrameComplete();
	UpdateCaptureScreenshot();

	// Stop gathering gfx stats.
	device.EndFrameStats();
	device.SynchronizeStats();
	device.GetFrameStats().AddToClientFrameTime (guiView->GetGameViewPresentTime());
	device.SetWireframe(false);
	#if ENABLE_PROFILER
	if (inPlayMode)
		UnityProfiler::Get().EndProfilingMode(kProfilerGame);
	#endif

	#if CAPTURE_SCREENSHOT_AVAILABLE
	if (AndroidHasRemoteConnected())
	{
		AndroidUpdateScreenShot(cameraRect);
	}
	if (RemoteIsConnected())
	{
		RemoteUpdateScreenShot(cameraRect);
	}
	#endif

	GetRenderManager().SetWindowRect (oldRenderRect);
	GetScreenManager().SetupScreenManagerEditor (oldEditorScreenWidth, oldEditorScreenHeight);	
	SetEditorMouseOffset( 0, 0, GetMainEditorWindow() );
}
