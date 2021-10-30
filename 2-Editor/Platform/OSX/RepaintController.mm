#include "UnityPrefix.h"
#include "Editor/Platform/Interface/RepaintController.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Input/GetInput.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Misc/CaptureScreenshot.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Misc/Player.h"
#include "Runtime/Misc/InputEvent.h"
#include "Runtime/Misc/UTF8.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Editor/Src/Application.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Profiler/CollectProfilerStats.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/DrawSplashScreenAndWatermarks.h"
#include "Editor/Src/Gizmos/GizmoManager.h"
#include "Editor/Src/RemoteInput/iPhoneRemoteImpl.h"
#include "Editor/Src/RemoteInput/AndroidRemote.h"
#include "Editor/Src/RemoteInput/GenericRemote.h"
#include "Runtime/GfxDevice/opengl/GLContext.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/BaseClasses/Cursor.h"
#import <Cocoa/Cocoa.h>

class Renderable;


using namespace std;

static set <NSView *> gDrawViews;

void AddSceneRepaintView (NSView *view)
{
	gDrawViews.insert (view);
}

void RemoveSceneRepaintView (NSView *view)
{
	gDrawViews.erase (view);
}

void RepaintAllSceneRepaintViews()
{
	for (set<NSView *>::iterator i = gDrawViews.begin(); i != gDrawViews.end(); i++)
	{
		SetCurrentView (*i);			// Tell GizmoUtils that Handles.Repaint should repaint _this_ view
		if ([*i lockFocusIfCanDraw])
		{
			[*i drawRect: [*i bounds]];
			[*i unlockFocus];
			[*i setNeedsDisplay: NO];
		}
	}
	SetCurrentView (NULL);
}



NSView * 	s_CurrentView = NULL;
void RepaintCurrentView () {
	AssertIf (!s_CurrentView);
	[s_CurrentView setNeedsDisplay:YES];
}

void SetCurrentView (/*NSView*/ void *currentView) {
	s_CurrentView = (NSView *)currentView;
}

void RepaintView (void * view) {
	[(NSView*)view display];
}


void* GetScreenParamsFromGameView(bool updateScreenManager, bool setActiveView, bool* outHasFocus, Rectf* outGUIRect, Rectf* outCameraRect)
{
	NSView* gameview;
	GetGameViewAndRectAndFocus(outGUIRect, &gameview, outHasFocus);

	if (gameview)
	{
		float viewHeight = [gameview frame].size.height;
		*outCameraRect = Rectf (outGUIRect->x, viewHeight - outGUIRect->GetBottom(), outGUIRect->Width(), outGUIRect->Height());
	}
	else
	{
		*outCameraRect = *outGUIRect;
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
	NSView* gameview = (NSView*)GetScreenParamsFromGameView(true, true, &hasFocus, &guiRect, &cameraRect);

	if (IsWorldPlaying ())
	{
		InputReadJoysticks();
		InputGetKeyboardIMEMode ();

		if (hasFocus)
		{
			// Set the mouse position
			NSPoint pos = [[gameview window] mouseLocationOutsideOfEventStream];
			pos = [gameview convertPoint: pos fromView: NULL];
			// If the game view uses constrained aspect ratios,
			// we need to convert the position based on the borders.
			pos.x -= cameraRect.x;
			pos.y -= cameraRect.y;
			GetInputManager ().SetMousePosition (Vector2f (pos.x, pos.y));

			InputReadMouseState ();
		}
	}
}


// These are not in the 10.4 SDK
enum {
    UNITY_NSEventTypeMagnify          = 30,
    UNITY_NSEventTypeSwipe            = 31,
    UNITY_NSEventTypeRotate           = 18,
};

void HandleGameViewInput (NSEvent* event)
{
	Rectf guiRect;
	NSView* gameView;
	bool hasFocus;
	GetGameViewAndRectAndFocus(&guiRect, &gameView, &hasFocus);

	int type = [event type];

	// Get mouse delta from cocoa just in case hid is not supported
	if (type == NSMouseMoved || type == NSLeftMouseDragged || type == NSRightMouseDragged || type == NSOtherMouseDragged)
		InputProcessMouseMove([event deltaX], [event deltaY]);

	if (type == NSMouseMoved)
	{
		SendMouseMoveEvent(event);
	}

	if (type == UNITY_NSEventTypeMagnify || type == UNITY_NSEventTypeSwipe || type == UNITY_NSEventTypeRotate)
	{
		InputEvent e (event, gameView);
		CallGlobalInputEvent(e);
	}

	if (type == NSKeyDown || type == NSKeyUp)
	{
		if( hasFocus && ([event modifierFlags] & NSCommandKeyMask))
		{
			//Cmd-key Events to GameView bypass responder chain, so we get Cmd-C, etc.
			InputEvent e (event, gameView);
			if(!((GUIOpenGLView*)gameView)->m_View->OnInputEvent (e))
				CallGlobalInputEvent(e);
		}

		if (IsWorldPlaying() && !GetApplication().IsPaused())
		{
			// Setup key state
			int scancode = [event keyCode];
			bool keyDown = [event type] == NSKeyDown;
			bool ignoreKeyPress = GetInputManager ().GetTextFieldInput() && GetInputManager().GetEatKeyPressOnTextFieldFocus();
			if (!hasFocus || ignoreKeyPress)
				keyDown = false;

			GetInputManager().SetKeyState (MapCocoaScancodeToSDL (scancode), keyDown);

			// Display cursor again when pressing escape
			if (MapCocoaScancodeToSDL ([event keyCode]) == SDLK_ESCAPE)
				GetScreenManager().SetAllowCursorLock(false);

			// Set the Entered Text
			if ([event type] == NSKeyDown)
			{
				NSString* chars = [event characters];
				for (int i=0;i<[chars length];i++)
				{
					string utf8;
					if( ConvertUTF16toUTF8 (NormalizeInputCharacter ([chars characterAtIndex: i]), utf8))
						GetInputManager().GetInputString() += utf8;
				}
			}
		}
	}
	else if (type == NSLeftMouseDown || type == NSRightMouseDown || type == NSOtherMouseDown || type == NSLeftMouseUp || type == NSRightMouseUp || type == NSOtherMouseUp)
	{
		bool inView = false;
		if(gameView)
		{
			NSPoint pos = [event locationInWindow];
			pos = [gameView convertPoint: pos fromView: NULL];
			float viewHeight = [gameView frame].size.height;
			Rectf cameraRect = Rectf (guiRect.x, viewHeight - guiRect.GetBottom(), guiRect.Width(), guiRect.Height());
			inView = cameraRect.Contains(pos.x,pos.y);
		}
		bool down = type == NSLeftMouseDown || type == NSRightMouseDown || type == NSOtherMouseDown;
		GetInputManager().SetMouseButton([event buttonNumber], down && hasFocus && inView);
	}
	else if (type == NSFlagsChanged)
	{
		UInt32 mod = [event modifierFlags];
		bool ignoreKeyPress = GetInputManager ().GetTextFieldInput() && GetInputManager().GetEatKeyPressOnTextFieldFocus();
		if (hasFocus && !ignoreKeyPress)
			GetKeyStateFromFlagsChangedEvent (mod);
	}
	else if (type == NSScrollWheel)
	{
		Vector3f delta = GetInputManager().GetMouseDelta();
		GetInputManager().SetMouseDelta(Vector3f (delta.x, delta.y, delta.z + [event deltaY]));
	}
}

void RenderGameViewCameras(const Rectf& guiRect, bool gizmos, bool gui)
{
	float screenHeight = GetRenderManager().GetWindowRect().Height();
	Rectf cameraRect = Rectf (guiRect.x, screenHeight - guiRect.GetBottom(), guiRect.Width(), guiRect.Height());

	// Since update & offscreen rendering is not done for each repaint (only when something changes),
	// we store gathered stats for that, and add actual repaint stats on top.
  	GfxDevice& device = GetGfxDevice();
	device.RestoreDrawStats();
    device.BeginFrameStats();

	#if ENABLE_PROFILER
	bool isPlaying = IsWorldPlaying() && !GetApplication().IsPaused();
	if (isPlaying)
		profiler_start_mode(kProfilerGame);
	#endif

	// Setup Rendering Rects
	GetScreenManager().SetupScreenManagerEditor( cameraRect.Width(), cameraRect.Height() );
	GetRenderManager().SetWindowRect (cameraRect);

	GUIView* guiView = GUIView::GetCurrent();
	NSView* cocoaView = guiView->GetCocoaView();

	SetSyncToVBL([guiView->GetCocoaView() context],GetQualitySettings().GetCurrent().vSyncCount);

	NSPoint p = NSMakePoint(guiRect.x + guiRect.width / 2, guiRect.y + guiRect.height / 2);
	p = [[cocoaView window] convertBaseToScreen: [cocoaView convertPoint:p toView:nil]];
	p.y = [[[cocoaView window]screen]frame].size.height - p.y;
	GetScreenManager().SetGlobalWindowCenter(p.x, p.y);

	if( gizmos )
		GetRenderManager().AddCameraRenderable( &GetGameViewGizmoRenderable(), kQueueIndexMax-1 );

	// Setup screen stats
	device.GetFrameStats().SetScreenParams( cameraRect.Width(), cameraRect.Height(), 4, 4, 4, device.GetCurrentTargetAA() );

	// Perform house keeping
	GetApplication().PerformDrawHouseKeeping();

	// offscreen cameras are drawn separately, before any views are rendered
	GetRenderManager().RenderCameras();

	// Repaint all GUI elements...
	///@TODO: hack to generate correct mouse position. We should pass int offsets explicitly intead of reading global state!!!
	GetRenderManager().SetWindowRect (guiRect);
	InputEvent e = InputEvent::RepaintEvent (cocoaView);
	GetRenderManager().SetWindowRect (cameraRect);

	if ( gui )
	{

	GetGUIManager().DoGUIEvent(e, false);
#if ENABLE_RETAINEDGUI
		GetGUITracker().Repaint();
#endif
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
	if (isPlaying)
		profiler_end_mode(kProfilerGame);
	#endif

#if SUPPORT_IPHONE_REMOTE && CAPTURE_SCREENSHOT_AVAILABLE
    if (AndroidHasRemoteConnected())
    {
        Rectf rc = cameraRect;
        AndroidUpdateScreenShot(cameraRect);
    }
    if (RemoteIsConnected())
    {
        Rectf rc = cameraRect;
        RemoteUpdateScreenShot(cameraRect);
    }

	if (iPhoneHasRemoteConnected())
	{
		Rectf rc = cameraRect;
		Image& buffer = iPhoneGetRemoteScreenShotBuffer();
		buffer.ReformatImage(rc.Width(), rc.Height(), kTexFormatRGBA32);
		GetGfxDevice().CaptureScreenshot( rc.x, rc.y, rc.Width(),
            rc.Height(), buffer.GetImageData() );
		iPhoneDidModifyScreenshotBuffer (true);
	}
#endif
}
