#ifndef __WEBVIEW_CONTROLLER_H__
#define __WEBVIEW_CONTROLLER_H__


#define WEBVIEW_IMPL_WEBKIT 1


typedef void* WebKeyboardEvent;

#if ENABLE_ASSET_STORE

#if WEBVIEW_IMPL_WEBKIT
#	if UNITY_WIN
#		include <WebKit/WebKit.h>
#		include <WebKit/WebKitCOMAPI.h>
		typedef IWebView* WebViewPtr;
		typedef IWebFrame* WebFramePtr;
		typedef HWND WebWindowPtr;

		class WebViewDelegate;
#	elif UNITY_OSX && defined(__OBJC__)
#		include "Runtime/Misc/InputEvent.h"
#		include <Cocoa/Cocoa.h>
#		include "Runtime/Input/InputManager.h"

#		import <WebKit/WebFrame.h>

#		import "Editor/Platform/OSX/WrappedWebViewWindow.h"
#		import "Editor/Platform/OSX/WebHTMLViewHacks.h"
		typedef WebView* WebViewPtr;
		typedef WebFrame* WebFramePtr;
		typedef WrappedWebViewWindow* WebWindowPtr;
#	else
		typedef void* WebViewPtr;
		typedef void* WebFramePtr;
		typedef void* WebWindowPtr;
#	endif
#endif

#endif // ENABLE_ASSET_STORE


#include <string.h>
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Shaders/GraphicsCaps.h"

class WebViewWrapper;
class WebScriptObjectWrapper;

extern GraphicsCaps gOriginalCaps;


struct MonoWebViewData {
	UnityEngineObjectMemoryLayout data;
	WebViewWrapper* m_WrapperPtr;
};

extern TextureFormat g_WebKitTextureFormat;
extern float g_WebKitFlipYScale;


class WebViewWrapper 
{
private: 
#if ENABLE_ASSET_STORE
#if UNITY_WIN
	// Used for non-DX rendering
	HBITMAP m_Bitmap;
	bool m_UseDirectX;

	HWND m_WebKitInternalWindow;
	HWND m_ParentWindow;
	WebViewDelegate* m_Delegate;

	// When we do SendMessage to WebKit window passing cursor position (e.g. where mouse was clicked) in some places webkit will take params passed to that function
	// in other cases it will get cursor position on screen directly from OS. For that reason we move the hidden WebKit window to match the position of the
	// texture we draw in the editor on screen. So the mouse handling is correct.
	POINT m_TextureScreenPosition;

	// used for testing for infinite inject message loop
	bool m_InjectingMessage;

	void CreateWindowForWebView();
	void InitWebKit();
	void InitWebView();
	void DoRenderImplGDI( );
	void DoRenderImplDirectX();
	void InjectMessage (UINT msg, WPARAM wParam, LPARAM lParam);
	LPARAM ConvertCursorPosition(int x, int y);
#endif // UNITY_WIN

	int m_Width;
	int m_Height;
	PPtr<MonoBehaviour> m_DelegateMonoObject;
	
	void DestroyCommon ();
	void DoRenderImpl( );
	void DoResizeImpl( int width, int height, bool textureSizeChanged );
	bool InvokeDelegateMethod(MonoMethodDesc* desc, void** argumentList, int inArgCount);

#if WEBVIEW_IMPL_WEBKIT 
	WebViewPtr m_WebView;
	WebFramePtr m_WebFrame;
	WebWindowPtr m_Window;
#endif
	
	
	// Allocate this object on the stack to temporarily set the maxTextureSize to the non-emulated size
	// Note: it assumes that you don't change emulation mode while it's active, so don't do that.
	class AutoDisableEmulation
	{
	private:
		GraphicsCaps m_SavedGraphicsCaps;
	public:
		AutoDisableEmulation()
		{
			m_SavedGraphicsCaps = gGraphicsCaps;
			gGraphicsCaps.InitializeOriginalEmulationCapsIfNeeded(); // force initialization of gOriginalCaps
			gGraphicsCaps = gOriginalCaps;
		}
		
		~AutoDisableEmulation()
		{
			gGraphicsCaps = m_SavedGraphicsCaps;
		}
	};
	void InitTexture(int width, int height);
	void PostInitTexture(int width, int height);

#endif // ENABLE_ASSET_STORE

	static WebViewWrapper* s_FocusedWebView;

public:
	Texture2D* m_TargetTexture;

	void SetWebkitControlsGlobalLocation(int x, int y);

	WebViewWrapper(int width, int height, bool showResizeHandle=false);	
	~WebViewWrapper() ;
	bool UpdateTexture();
	bool IsDirty();
	void Resize(int width, int height);
	void SetDelegateMonoObject(MonoBehaviour* object);

	void InjectMouseMove(int x, int y);
	void InjectMouseDrag(int x, int y, int button);
	void InjectMouseDown(int x, int y, int button, int clickCount);
	void InjectMouseUp(int x, int y, int button, int clickCount);
	void InjectMouseWheel(int x, int y, float deltaX, float deltaY);
	void InjectKeyboardEvent(const WebKeyboardEvent& keyboardEvent);
	void Focus();
	void UnFocus();
	void LoadURL(const string& url);
	void LoadFile(const string& path);

	void OnBeginNavigation(  const std::string& url, const std::string& frameName);
	void OnBeginLoading( const std::string& url, const std::string& frameName, const std::string& mimeType);
	void OnFinishLoading( );
	void OnReceiveTitle( const std::string& title, const std::string& frameName);
	void OnLoadError( const std::string& frameName);
	void OnOpenExternalLink( const std::string& url, const std::string& source );
	void SetCursor( int cursor);
	void OnWebViewDirty();
	void Copy ();
	void Cut ();
	void Paste ();
	void SelectAll ();
	void Undo ();
	void Redo ();
	bool HasUndo ();
	bool HasRedo ();
	
	WebScriptObjectWrapper* GetWindowScriptObject() ;

	static WebKeyboardEvent GetCurrentKeyboardEvent();
	static WebViewWrapper *GetFocusedWebView() { return s_FocusedWebView; }
	
	#if UNITY_WIN
	void Unparent();
	void Reparent();
	static std::wstring GetWindowClassName();
	#endif // UNITY_WIN
	
};


#endif 
