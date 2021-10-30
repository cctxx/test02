#include "UnityPrefix.h"


#include "Editor/Src/LicenseActivationWindowCustomJS.h"
#include "Editor/Src/LicenseInfo.h"

#if ENABLE_ASSET_STORE

#include "Editor/src/WebViewWrapper.h"
#include "Editor/Src/Utility/CurlRequest.h"
#include "Editor/Platform/Windows/Utility/SplashScreen.h"
#include "Editor/Platform/Windows/LicenseWebViewWindow.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "PlatformDependent/Win/WinUnicode.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Runtime/Utilities/URLUtility.h"
#include <comutil.h>
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Allocator/MemoryManager.h"

namespace
{
	LicenseWebViewWindow* gSingleton = NULL;
	bool licenseWindowClassRegistered = false;
	bool toggleSplashScreen = false;

	LRESULT CALLBACK LicenseWebViewProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		switch (Msg) 
		{ 
		case WM_CLOSE: 
			gSingleton->ExpectError();
			LicenseInfo::Get()->SignalUserClosedWindow();
			return 0;
		} 

		return DefWindowProcW(hWnd, Msg, wParam, lParam); 
	}
}

void LicenseLog (const char* format, ...);


void ProcessMessages()
{
	MSG  msg;

	while (PeekMessageW (&msg, NULL,  0, 0, PM_REMOVE)) 
	{
		if( msg.message != WM_RBUTTONUP && msg.message != WM_RBUTTONDOWN )
		{
			TranslateMessage( &msg );
			DispatchMessageW( &msg );
		}
	}

	CurlRequestCheck();
	LicenseInfo::Get()->Tick();
	::Sleep(100);  // Leave some time for other processes
}

void TurnOffSplashScreenWhenActivationWindowAppears()
{
	toggleSplashScreen = true;
}

bool LicenseMessagePump()
{
	GetMemoryManager().ThreadInitialize();

	while (LicenseInfo::Get()->WindowIsOpen() || !LicenseInfo::Get()->Activated())
	{
		ProcessMessages();
	}

	toggleSplashScreen = false;
	return LicenseInfo::Get()->Activated();
}

void LicenseInfo::CreateLicenseActivationWindow()
{
	Assert(gSingleton == NULL);
	gSingleton = new LicenseWebViewWindow();
	m_DidCreateWindow = true;
	LicenseMessagePump();
}

void LicenseInfo::DestroyLicenseActivationWindow()
{
	delete gSingleton;
	gSingleton = NULL;
	m_DidCreateWindow = false;
}


HWND GetMainEditorWindow();

LicenseWebViewWindow::LicenseWebViewWindow(): m_Width(640), m_Height(550), m_ExpectError(false)
{
	std::wstring ActivationWindowClass = L"ActivationWindowClass";
	if (!licenseWindowClassRegistered)
	{
		winutils::RegisterWindowClass(ActivationWindowClass.c_str(), LicenseWebViewProc, CS_HREDRAW | CS_VREDRAW);
		licenseWindowClassRegistered = true;
	}

	m_Window = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE, ActivationWindowClass.c_str(), NULL, WS_CAPTION | WS_SYSMENU, 0, 0, m_Width, m_Height, NULL , NULL, winutils::GetInstanceHandle(), NULL);

	SetForegroundWindow(m_Window);
	EnableWindow(GetMainEditorWindow(), false);

	WebKitCreateInstance(CLSID_WebView, 0, IID_IWebView, (void**)&m_WebView);
	m_WebView->setHostWindow((OLE_HANDLE)m_Window);


	RECT rect;
	rect.top = 0;
	rect.left = 0;
	rect.right = m_Width;
	rect.bottom = m_Height;
	m_WebView->initWithFrame(rect, NULL, NULL);

	_bstr_t str("Unity/" UNITY_VERSION " (http://unity3d.com)");
	m_WebView->setApplicationNameForUserAgent(str);
	m_WebView->setDrawsBackground(false);
	m_WebView->setMaintainsBackForwardList(false);

	m_WebView->setUIDelegate(this);
	m_WebView->setFrameLoadDelegate(this);
	m_WebView->setPolicyDelegate(this);
	m_WebView->setResourceLoadDelegate(this);

	m_WebView->mainFrame(&m_WebFrame);
	m_WebFrame->setAllowsScrolling(true);

	IWebMutableURLRequest *request = 0;
	WebKitCreateInstance(CLSID_WebMutableURLRequest, 0, IID_IWebMutableURLRequest, (void**)&request);
	_bstr_t burl(LicenseInfo::Get()->GetLicenseURL().c_str());
	_bstr_t bget(L"GET");

	IWebScriptObject* webScriptObject;
	m_WebView->windowScriptObject(&webScriptObject);

	HRESULT hr = request->initWithURL(burl, (WebURLRequestCachePolicy)WebURLRequestUseProtocolCachePolicy, 60);

	hr = request->setHTTPMethod(bget);
	m_WebFrame->loadRequest(request);

	if (request)
		request->Release();

	m_windowVisible = false;
}

LicenseWebViewWindow::~LicenseWebViewWindow()
{
	if (toggleSplashScreen)
		ShowSplashScreen();

	// Make sure the splashscreen is only toggled off/on when the appropriate function has been called 
	toggleSplashScreen = false;

	ShowWindow(m_Window, SW_HIDE);
	EnableWindow(GetMainEditorWindow(), true);
	m_WebFrame->Release();
	m_WebView->Release();
	DestroyWindow(m_Window);
}

void LicenseWebViewWindow::ExpectError()
{
	m_ExpectError = true;
}


HRESULT STDMETHODCALLTYPE LicenseWebViewWindow::didReceiveResponse( /* [in] */ IWebView *webView, /* [in] */ unsigned long identifier, /* [in] */ IWebURLResponse *response, /* [in] */ IWebDataSource *dataSource )
{
	HRESULT hr = S_OK;

	BSTR url;
	BSTR mimeType;
	BSTR frameName;
	IWebFrame* frame = NULL;

	if ( FAILED( hr = response->URL( &url ) ) )
		goto error;

	if ( FAILED( hr = response->MIMEType( &mimeType ) ) )
		goto error;

	if ( FAILED( hr = dataSource->webFrame( &frame ) ) )
		goto error;

	if ( FAILED( hr = frame->name( &frameName ) ) )
		goto error;

	int status = 0;
	IWebHTTPURLResponse* httpResponse;
	if (SUCCEEDED(response->QueryInterface(&httpResponse)))
		httpResponse->statusCode( &status);

	IPropertyBag* pPropBag;
	hr = ((IWebHTTPURLResponse*)response)->allHeaderFields(&pPropBag);

	IPropertyBag2 * pPropBag2;
	hr = pPropBag->QueryInterface( IID_IPropertyBag2, (void**)&pPropBag2 );

	unsigned long n;
	hr = pPropBag2->CountProperties (&n);

	PROPBAG2 * aPropNames = new PROPBAG2[n];
	unsigned long aReaded;

	hr = pPropBag2->GetPropertyInfo (0, n, aPropNames, & aReaded);

	CComVariant * aVal = new CComVariant[n];
	HRESULT * hvs = new HRESULT[n];
	hr = pPropBag2->Read (n, aPropNames, NULL, aVal, hvs);

	for (unsigned long i = 0; i < n; i++)
	{
		USES_CONVERSION;
		string key( OLE2CT(aPropNames[i].pstrName) );
		string val( OLE2CA(V_BSTR(&aVal[i])) );

		if (key == "X-RX")
			LicenseInfo::Get()->SetRXValue(val.c_str());
	}

	delete[] hvs;
	delete[] aVal;
	delete[] aPropNames;


error:
	if ( frame )
		frame->Release ();
	return hr;
}




HRESULT STDMETHODCALLTYPE LicenseWebViewWindow::didFinishLoadForFrame(IWebView *webView, IWebFrame *frame )
{
	if (!m_windowVisible)
	{
		const int ScreenX = (GetSystemMetrics(SM_CXSCREEN) - m_Width) / 2;
		const int ScreenY = (GetSystemMetrics(SM_CYSCREEN) - m_Height) / 2;
		SetWindowPos(m_Window, NULL, ScreenX, ScreenY, m_Width, m_Height, 0);
		if (toggleSplashScreen)
			HideSplashScreen();

		ShowWindow(m_Window, SW_SHOW);
		m_windowVisible = true;
	}

	JSGlobalContextRef globalContext = frame->globalContext();
	AddJSClasses(globalContext);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE LicenseWebViewWindow::didReceiveTitle(IWebView *webView, BSTR title, IWebFrame *frame )
{
	SetWindowTextW(m_Window, title);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE LicenseWebViewWindow::decidePolicyForNewWindowAction(IWebView *webView, IPropertyBag *actionInformation, IWebURLRequest *request, BSTR frameName, IWebPolicyDecisionListener *listener)
{
	BSTR url;

	listener->ignore();

	if ( SUCCEEDED ( request->URL( &url) ) )
		OpenURL(string (WideToUtf8 (url)));

	return S_OK;
}

HRESULT STDMETHODCALLTYPE LicenseWebViewWindow::didFailLoadingWithError(IWebView *webView, unsigned long identifier, IWebError *error, IWebDataSource *dataSource)
{
	if (!m_ExpectError)
		HandleError(webView, error);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE LicenseWebViewWindow::didFailLoadWithError(IWebView *webView, IWebError *error, IWebFrame *forFrame)
{
	if (!m_ExpectError)
		HandleError(webView, error);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE LicenseWebViewWindow::didFailProvisionalLoadWithError(IWebView *webView, IWebError *error, IWebFrame *frame)
{
	HandleError(webView, error);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE LicenseWebViewWindow::webViewAddMessageToConsole(IWebView *sender, BSTR message, int lineNumber, BSTR url, BOOL isError)
{
	int t = kLog;
	if ( isError )
		t = kError;
	
	DebugStringToFile(string (WideToUtf8 (message)), 0, string (WideToUtf8 (url)).c_str(), lineNumber, t);
	return S_OK;
}

void LicenseWebViewWindow::HandleError(IWebView *webView, IWebError* error)
{
	BSTR errorString;
	BSTR url;
	if (SUCCEEDED(error->localizedDescription(&errorString)) 
		&& SUCCEEDED(error->failingURL(&url)) 
		&& errorString != NULL && url != NULL)
	{
		std::string errorMsg = WideToUtf8(errorString);
		LicenseLog("Error: %s for %s\n", errorMsg.c_str(), WideToUtf8(url).c_str());
		DisplayDialog("Error loading page", errorMsg, "OK");
		LicenseInfo::Get()->SignalUserClosedWindow();
	}
	else
		LicenseLog("WebKit loading problem\n");
}

#else // ENABLE_ASSET_STORE

void TurnOffSplashScreenWhenActivationWindowAppears() { }
void LicenseInfo::CreateLicenseActivationWindow() { }
void LicenseInfo::DestroyLicenseActivationWindow() { }
bool LicenseMessagePump() { return true; }

#endif // ENABLE_ASSET_STORE
