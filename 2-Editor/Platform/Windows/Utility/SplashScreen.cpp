#include "UnityPrefix.h"
#include "SplashScreen.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "../resource.h"
#include "PlatformDependent/Win/WinUtils.h"

// -----------------------------------------------------------------------------

static const wchar_t* kSplashScreenClass = L"UnitySplashWindow";

static HWND s_SplashWindow;
static HBITMAP s_SplashBitmap;



// We have to setup splash screen parent before hiding it.
// Otherwise when we're destroying the splash screen, Windows will activate some
// previous application because the parent of the window was null.
void SetSplashScreenParent( HWND window )
{
	if( !s_SplashWindow )
		return;
	HWND parent = GetParent(s_SplashWindow);
	SetParent( parent, window );
}


static HWND CreateSplashWindow()
{
	HINSTANCE instance = winutils::GetInstanceHandle();

	WNDCLASSW wc = { 0 };
	wc.lpfnWndProc = DefWindowProcW;
	wc.hInstance = instance;
	wc.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APP_ICON));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = kSplashScreenClass;
	RegisterClassW(&wc);

	HWND hwndOwner = CreateWindowW(kSplashScreenClass, NULL, WS_POPUP, 0, 0, 0, 0, NULL, NULL, instance, NULL);
	return CreateWindowExW(WS_EX_LAYERED, kSplashScreenClass, NULL, WS_POPUP | WS_VISIBLE, 0, 0, 0, 0, hwndOwner, NULL, instance, NULL);
}

static void SetSplashImage(HWND hwndSplash, UINT resourceID, int ytopFromCenter)
{
	// get the primary monitor's info
	POINT ptZero = { 0 };
	HMONITOR hmonPrimary = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO monitorinfo = { 0 };
	monitorinfo.cbSize = sizeof(monitorinfo);
	GetMonitorInfo(hmonPrimary, &monitorinfo);

	// create a memory DC holding the splash bitmap
	HDC hdcScreen = GetDC(NULL);
	HDC hdcMem = CreateCompatibleDC(hdcScreen);

	// load the bitmap
	int splashWidth, splashHeight;
	HBITMAP hbmpSplash = LoadPNGFromResource( hdcMem, resourceID, splashWidth, splashHeight );
	s_SplashBitmap = hbmpSplash;
	HBITMAP hbmpOld = (HBITMAP) SelectObject(hdcMem, hbmpSplash);

	// use the source image's alpha channel for blending
	BLENDFUNCTION blend = { 0 };
	blend.BlendOp = AC_SRC_OVER;
	blend.SourceConstantAlpha = 255;
	blend.AlphaFormat = AC_SRC_ALPHA;

	// center the splash screen in the middle of the primary work area
	const RECT & rcWork = monitorinfo.rcWork;
	POINT ptOrigin;
	ptOrigin.x = rcWork.left + (rcWork.right - rcWork.left - splashWidth) / 2;
	ptOrigin.y = (rcWork.bottom - rcWork.top) / 2 - ytopFromCenter;

	// paint the window (in the right location) with the alpha-blended bitmap
	SIZE sizeSplash = { splashWidth, splashHeight };
	UpdateLayeredWindow(hwndSplash, hdcScreen, &ptOrigin, &sizeSplash, hdcMem, &ptZero, RGB(255, 0, 255), &blend, ULW_ALPHA);

	// delete temporary objects
	SelectObject(hdcMem, hbmpOld);
	DeleteDC(hdcMem);
	ReleaseDC(NULL, hdcScreen);
}

void ShowSplashScreen()
{
	AssertIf(s_SplashWindow || s_SplashBitmap);
	s_SplashWindow = CreateSplashWindow();
	SetSplashImage( s_SplashWindow, IDR_SPLASH_IMAGE, 200 );
}

void HideSplashScreen()
{
	if( s_SplashWindow )
	{
		HWND parent = GetParent(s_SplashWindow);
		DestroyWindow(s_SplashWindow);
		DestroyWindow(parent);
		s_SplashWindow = NULL;
	}
	if( s_SplashBitmap )
	{
		DeleteObject(s_SplashBitmap);
		s_SplashBitmap = NULL;
	}
}
