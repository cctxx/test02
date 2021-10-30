#include "UnityPrefix.h"


#include "Runtime/Utilities/File.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/BitUtility.h"
#include "PlatformDependent/Win/WinUtils.h"

#include "Editor/Src/WebViewWrapper.h"

#if ENABLE_ASSET_STORE

#include "Editor/Src/WebViewScripting.h"
#include "Editor/Platform/Windows/WebKit/WebViewDelegate.h"
#include "Editor/Platform/Interface/EditorWindows.h"

#include "Runtime/Utilities/FileUtilities.h"
#include "Configuration/UnityConfigureVersion.h"

#include <comutil.h>
#include <atlbase.h>

static std::wstring s_WebKitWindowClass;


static HDC AcquireHDCForTexture (TextureID tid, int& outWidth, int& outHeight)
{
	HDC AcquireHDCForTextureD3D9 (TextureID tid, int& outWidth, int& outHeight);
	HDC AcquireHDCForTextureD3D11 (TextureID tid, int& outWidth, int& outHeight);

	const GfxDeviceRenderer type = GetGfxDevice().GetRenderer();
	if (type == kGfxRendererD3D9)
		return AcquireHDCForTextureD3D9 (tid, outWidth, outHeight);
	else if (type == kGfxRendererD3D11)
		return AcquireHDCForTextureD3D11 (tid, outWidth, outHeight);
	else
		return NULL;
}

static void ReleaseHDCForTexture (TextureID tid, HDC dc)
{
	void ReleaseHDCForTextureD3D9 (TextureID tid, HDC dc);
	void ReleaseHDCForTextureD3D11 (TextureID tid, HDC dc);
	const GfxDeviceRenderer type = GetGfxDevice().GetRenderer();
	if (type == kGfxRendererD3D9)
		ReleaseHDCForTextureD3D9 (tid, dc);
	else if (type == kGfxRendererD3D11)
		ReleaseHDCForTextureD3D11 (tid, dc);
}


WebViewWrapper::WebViewWrapper(int width, int height, bool showResizeHandle) :
m_Bitmap(NULL),
m_UseDirectX(false),
m_Width(width), 
m_Height(height), 
m_WebView(NULL),
m_Window(NULL),
m_ParentWindow(NULL),
m_TargetTexture(NULL),
m_InjectingMessage(false)
{
	InitWebKit();

	CreateWindowForWebView();
	InitWebView();

	_bstr_t str("Unity/" UNITY_VERSION " (http://unity3d.com)");
	m_WebView->setApplicationNameForUserAgent(str);
	m_WebView->setMaintainsBackForwardList((BOOL)false);

	InitTexture(width, height);
}

WebViewWrapper::~WebViewWrapper() 
{

	if (m_Delegate)
		m_Delegate->Release();
	if (m_WebFrame)
		m_WebFrame->Release();
	if (m_WebView)
	{
		m_WebView->setUIDelegate(NULL);
		m_WebView->setFrameLoadDelegate(NULL);
		m_WebView->setPolicyDelegate(NULL);
		m_WebView->setResourceLoadDelegate(NULL);
		m_WebView->Release();
	}

	SetWindowLongPtr( (HWND) m_Window, GWLP_USERDATA, (LONG_PTR) NULL );
	CloseWindow((HWND)m_Window);

	m_WebView=NULL;
	m_WebView=NULL;
	m_Window=NULL;
	m_Delegate=NULL;

	DestroyCommon ();
	//OleUninitialize();
}

WebKeyboardEvent WebViewWrapper::GetCurrentKeyboardEvent()
{
	return NULL;
}

void WebViewWrapper::InitWebKit()
{
}

void WebViewWrapper::InitWebView()
{
	const GfxDeviceRenderer type = GetGfxDevice().GetRenderer();
	m_UseDirectX = (type == kGfxRendererD3D9 || type == kGfxRendererD3D11);
	if (m_UseDirectX)
		g_WebKitTextureFormat = kTexFormatRGB24;
	else
		g_WebKitFlipYScale = 1;

	if (FAILED(WebKitCreateInstance(CLSID_WebView, 0, IID_IWebView, (void**)&m_WebView)))
		Scripting::RaiseMonoException("Failed to create WebView");

	m_WebView->setHostWindow(HandleToLong((void*)m_Window));

	RECT rect;
	rect.top = 0;
	rect.left = 0;
	rect.right = m_Width;
	rect.bottom = m_Height;

	m_WebView->initWithFrame(rect, NULL, NULL);

	IWebViewPrivate *webViewPrivate;
	if (FAILED(m_WebView->QueryInterface(IID_IWebViewPrivate,(void**) &webViewPrivate)))
		Scripting::RaiseMonoException("Failed to query WebViewPrivate interface");

	if (webViewPrivate)
	{
		OLE_HANDLE win = 0;
		webViewPrivate->viewWindow(&win);
		m_WebKitInternalWindow = (HWND)win;
		webViewPrivate->Release();
	}

	if (FAILED(m_WebView->mainFrame(&m_WebFrame)))
		Scripting::RaiseMonoException("Failed to get webView main frame");

	m_Delegate = new WebViewDelegate(this);

	m_WebView->setUIDelegate((IWebUIDelegate*) m_Delegate);
	m_WebView->setFrameLoadDelegate((IWebFrameLoadDelegate*) m_Delegate);
	m_WebView->setPolicyDelegate((IWebPolicyDelegate*) m_Delegate);
	m_WebView->setResourceLoadDelegate((IWebResourceLoadDelegate*) m_Delegate);

}

std::wstring WebViewWrapper::GetWindowClassName()
{
	if (s_WebKitWindowClass.empty())
	{
		s_WebKitWindowClass = L"WebKitWindowClass";
		winutils::RegisterWindowClass(s_WebKitWindowClass.c_str(), DefWindowProcW/*WebViewWrapper::WrapperWndProc*/, CS_HREDRAW | CS_VREDRAW);
	}
	return s_WebKitWindowClass;
}

void WebViewWrapper::CreateWindowForWebView()
{
	GetWindowClassName();
	m_ParentWindow = GUIView::GetCurrent()->GetWindowHandle();

	if (!(m_Window = CreateWindowW(s_WebKitWindowClass.c_str(), NULL, WS_CHILD/*WS_OVERLAPPEDWINDOW */, 0, 0, m_Width, m_Height, m_ParentWindow , NULL, winutils::GetInstanceHandle(), NULL)))
		Scripting::RaiseMonoException("Call to CreateWindow failed! %s",  winutils::ErrorCodeToMsg(GetLastError()).c_str());
	SetWindowLongPtr( (HWND) m_Window, GWLP_USERDATA, (LONG_PTR) this );
}

// Called whenever the parent container window is going away
void WebViewWrapper::Unparent( )
{
	ClientToScreen(m_ParentWindow, &m_TextureScreenPosition);
	m_ParentWindow=HWND_MESSAGE;
	SetParent(m_Window, HWND_MESSAGE);
}

// Called to ensure that we are always the child window of the current guiview
void WebViewWrapper::Reparent( )
{
	HWND current = GUIView::GetCurrent()->GetWindowHandle();
	if (m_ParentWindow != current) {
		
		// Remap screen position to current parent window
		if( current != HWND_MESSAGE)
			ClientToScreen(m_ParentWindow, &m_TextureScreenPosition);
		ScreenToClient(current, &m_TextureScreenPosition);
		
		m_ParentWindow=current;
		SetParent(m_Window, m_ParentWindow);
	}
}


void WebViewWrapper::LoadURL(const string& url)
{
	HRESULT hr;

	IWebMutableURLRequest *request = 0;
    if (FAILED(WebKitCreateInstance(CLSID_WebMutableURLRequest, 0, IID_IWebMutableURLRequest, (void**)&request)))
		Scripting::RaiseMonoException("Failed to create URL request");

	_bstr_t burl(url.c_str());
	_bstr_t bget(L"GET");

	hr=request->initWithURL(burl, (WebURLRequestCachePolicy)WebURLRequestUseProtocolCachePolicy, 60);
	if (FAILED(hr))
		Scripting::RaiseMonoException("Failed to initialize URL request");
		
	hr=request->setHTTPMethod(bget);
	if (FAILED(hr))
		Scripting::RaiseMonoException("Failed to initialize URL request");

	if (FAILED(m_WebFrame->loadRequest(request)))
		Scripting::RaiseMonoException("Failed to navigate to web page");

	if (request)
		request->Release();
}

void WebViewWrapper::LoadFile(const string& path)
{
	LoadURL("file:///"+path); // Note: This might require a fix for case 382801 if confirmed on windows
}

void WebViewWrapper::DoResizeImpl( int width, int height, bool textureSizeWasChanged )
{
	if (!m_UseDirectX)
	{
		MoveWindow((HWND)m_WebKitInternalWindow, 0, 0, width, height, false);
		MoveWindow((HWND)m_Window, m_TextureScreenPosition.x,m_TextureScreenPosition.y,width, height, false);

		if (m_Bitmap && textureSizeWasChanged )
		{
			DeleteObject(m_Bitmap);
			m_Bitmap = NULL;
		}
	}
	else
	{
		if ( textureSizeWasChanged )
		{
			m_TargetTexture->UpdateImageDataDontTouchMipmap(); // touch texture so it gets set up properly
		}

		MoveWindow((HWND)m_Window, m_TextureScreenPosition.x,m_TextureScreenPosition.y,width, m_TargetTexture->GetGLHeight(), false);
		MoveWindow((HWND)m_WebKitInternalWindow, 0, m_TargetTexture->GetGLHeight() - height, width, height, false);
	}

	UpdateTexture();

	m_Delegate->SetDirty(true);
}

void WebViewWrapper::DoRenderImpl() 
{
	if (m_UseDirectX)
		DoRenderImplDirectX();
	else
		DoRenderImplGDI();
}

void WebViewWrapper::DoRenderImplGDI() 
{
	ImageReference image;
	if( !m_TargetTexture->GetWriteImageReference(&image, 0, 0) )
	{
		ErrorString("Unable to retrieve image reference");
		return;
	}

	HWND hWnd = (HWND)m_Window;

	HDC hDCMem = CreateCompatibleDC(NULL);

	int paddedWidth = NextPowerOfTwo(m_Width);
	int paddedHeight = NextPowerOfTwo(m_Height);

	if (!m_Bitmap)
	{
		HDC hDC = GetDC(hWnd);
		m_Bitmap = CreateCompatibleBitmap(hDC, paddedWidth, paddedHeight);
		ReleaseDC(hWnd, hDC);
	}

	HGDIOBJ hOld = SelectObject(hDCMem, m_Bitmap);
	SendMessage(hWnd, WM_PRINT, (WPARAM) hDCMem, PRF_CHILDREN | PRF_CLIENT | PRF_OWNED);

	BITMAPINFO bi;
	ZeroMemory(&bi, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = paddedWidth;
	bi.bmiHeader.biHeight = m_Height;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	GetDIBits(hDCMem, m_Bitmap, 0, m_Height, image.GetImageData(), &bi, DIB_RGB_COLORS);

	ColorRGBA32* col = reinterpret_cast<ColorRGBA32*>(image.GetImageData());
	for( size_t i = 0; i < paddedWidth*m_Height; ++i )
		col[i] = col[i].SwizzleToBGR();

	SelectObject(hDCMem, hOld);
	DeleteObject(hDCMem);

	// I don't think this is a good way to pass title...
	/*char text[4096];
	GetWindowText((HWND)m_Window, text, 4096);
	m_TargetTexture->SetName(text);*/

	m_Delegate->SetDirty(false);
	m_TargetTexture->UpdateImageDataDontTouchMipmap();
	
}

void WebViewWrapper::DoRenderImplDirectX() 
{
	int dummyW, dummyH;
	GetGfxDevice().AcquireThreadOwnership();
	HDC hdc = AcquireHDCForTexture(m_TargetTexture->GetTextureID(), dummyW, dummyH);
	if (!hdc)
	{
		GetGfxDevice().ReleaseThreadOwnership();
		return;
	}
	SendMessage(m_Window, WM_PRINT, (WPARAM) hdc, PRF_CHILDREN | PRF_CLIENT | PRF_OWNED);
	ReleaseHDCForTexture(m_TargetTexture->GetTextureID(), hdc);
	GetGfxDevice().ReleaseThreadOwnership();
	m_Delegate->SetDirty(false);
}

LPARAM WebViewWrapper::ConvertCursorPosition(int x, int y)
{
	// If passed arguments are other than real cursor position on screen, mouse wont work correctly anyways. So get it directly. 
	// (read more next to definition of m_TextureScreenPosition)
	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient((HWND)m_WebKitInternalWindow, &pt);
	return MAKELPARAM(pt.x, pt.y);
}

void WebViewWrapper::PostInitTexture(int width, int height)
{
	if (m_UseDirectX)
		DoResizeImpl(width, height, true);
	else
		UpdateTexture();
}

void WebViewWrapper::SetWebkitControlsGlobalLocation(int x, int y)
{
	m_TextureScreenPosition.x = x;
	m_TextureScreenPosition.y = y;
	if (m_UseDirectX)
		m_TextureScreenPosition.y -= m_TargetTexture->GetGLHeight() - m_Height;
	if (m_ParentWindow != HWND_MESSAGE)
		ScreenToClient(m_ParentWindow, &m_TextureScreenPosition);
}

void WebViewWrapper::InjectMessage (UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Windows 8.1 introduced a regression in behaviour where some injected events will bubble back up to our window which leads to an infinite message loop
	// Due to timing with 4.3, Windows 8.1, and Gecko implementation we decided to not risk fixing this in Webkit, but rather early out if we detect this happening.
	// See case http://fogbugz.unity3d.com/default.asp?564852
	if (m_InjectingMessage) return;
	m_InjectingMessage = true;
	
	SendMessage(m_WebKitInternalWindow, msg, wParam, lParam);

	m_InjectingMessage = false;
}

void WebViewWrapper::InjectMouseDown(int x, int y, int button, int clickCount) 
{
	Reparent();
	MoveWindow(m_Window, m_TextureScreenPosition.x, m_TextureScreenPosition.y, m_Width, m_Height, false);

	int message = 0;
	switch (button)
	{
	case 0: message = WM_LBUTTONDOWN;break;
	case 1: message = WM_RBUTTONDOWN;break;
	case 2: message = WM_MBUTTONDOWN;break;
	}
	InjectMessage(message, 0, ConvertCursorPosition(x,y));
}

void WebViewWrapper::InjectMouseUp(int x, int y, int button, int clickCount) 
{
	Reparent();
	MoveWindow(m_Window, m_TextureScreenPosition.x, m_TextureScreenPosition.y, m_Width, m_Height, false);

	int message = 0;
	switch (button)
	{
	case 0: message = WM_LBUTTONUP;break;
	case 1: message = WM_RBUTTONUP;break;
	case 2: message = WM_MBUTTONUP;break;
	}
	InjectMessage(message, 0, ConvertCursorPosition(x,y));
}

void WebViewWrapper::InjectMouseMove(int x, int y) 
{
	Reparent();
	MoveWindow(m_Window, m_TextureScreenPosition.x, m_TextureScreenPosition.y, m_Width, m_Height, false);
	InjectMessage(WM_MOUSEMOVE, 0, ConvertCursorPosition(x,y));
}

void WebViewWrapper::InjectMouseWheel(int x, int y, float deltaX, float deltaY) 
{
	Reparent();

	//FIXME: when WebKit window is active it eats some events directly, rather than through these injects
	// adjusted delta multiplier to be same as internally on WebKit, though we could get into trouble in some other place
	InjectMessage(WM_MOUSEWHEEL, MAKEWPARAM(0, -deltaY*40), ConvertCursorPosition(x,y));
}

void WebViewWrapper::InjectKeyboardEvent(const WebKeyboardEvent& keyboardEvent) 
{
	Reparent();
	InputEvent kbdEvent = ExtractMonoObjectData<InputEvent>((MonoObject*)keyboardEvent);

	int message = 0;
	switch (kbdEvent.type)
	{
	case InputEvent::kKeyDown: message = WM_KEYDOWN; break;
	case InputEvent::kKeyUp: message = WM_KEYUP; break;
	default: return;
	}
	SetFocus(m_WebKitInternalWindow); //TODO: this doesn't belong here
	InjectMessage(message, kbdEvent.keycode, 0);
}

void WebViewWrapper::InjectMouseDrag(int x, int y, int button)  { }

bool WebViewWrapper::IsDirty()
{
	return m_Delegate->IsDirty();
}

void WebViewWrapper::SelectAll ()
{
	IWebDocumentText *webDocumentText;
	if (FAILED(m_WebFrame->QueryInterface(IID_IWebDocumentText,(void**) &webDocumentText)))
		Scripting::RaiseMonoException("Failed to query WebDocumentText interface");

	if (webDocumentText)
		webDocumentText->selectAll();
}

void WebViewWrapper::Copy ()
{
	IWebViewEditingActions *webViewEditingActions;
	if (FAILED(m_WebView->QueryInterface(IID_IWebViewEditingActions,(void**) &webViewEditingActions)))
		Scripting::RaiseMonoException("Failed to query WebViewEditingActions interface");

	if (webViewEditingActions)
		webViewEditingActions->copy(NULL);
}

void WebViewWrapper::Cut ()
{
	IWebViewEditingActions *webViewEditingActions;
	if (FAILED(m_WebView->QueryInterface(IID_IWebViewEditingActions,(void**) &webViewEditingActions)))
		Scripting::RaiseMonoException("Failed to query WebViewEditingActions interface");

	if (webViewEditingActions)
		webViewEditingActions->cut(NULL);
}

void WebViewWrapper::Paste ()
{
	IWebViewEditingActions *webViewEditingActions;
	if (FAILED(m_WebView->QueryInterface(IID_IWebViewEditingActions,(void**) &webViewEditingActions)))
		Scripting::RaiseMonoException("Failed to query WebViewEditingActions interface");
	HRESULT err;
	if (webViewEditingActions)
		webViewEditingActions->paste(NULL);
}

void WebViewWrapper::Undo ()
{
}

void WebViewWrapper::Redo ()
{
}

bool WebViewWrapper::HasUndo ()
{
	return false;
}

bool WebViewWrapper::HasRedo ()
{
	return false;
}

WebScriptObjectWrapper* WebViewWrapper::GetWindowScriptObject()
{
	IWebFrame* frame;
	HRESULT hr = m_WebView->mainFrame(&frame);
	return new WebScriptObjectWrapper( frame->globalContext() );
}

#endif // ENABLE_ASSET_STORE
