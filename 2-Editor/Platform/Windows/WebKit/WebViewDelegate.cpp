#include "UnityPrefix.h"

#if ENABLE_ASSET_STORE

#include "WebViewDelegate.h"
#include "PlatformDependent/Win/WinUnicode.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/Utilities/Argv.h"

static std::string ToString(BSTR b)
{
	if (! b ) return string(); // It's valid for a BSTR to be NULL, which is a special case for an empty string
	return string (WideToUtf8 (b));
}

WebViewDelegate::WebViewDelegate(WebViewWrapper * w)
: m_Wrapper(w)
, m_Dirty(true)
, m_ExpectError(false)
{
}

WebViewDelegate::~WebViewDelegate()
{
}

HRESULT STDMETHODCALLTYPE WebViewDelegate::didReceiveTitle( 
	/* [in] */ IWebView *webView,
	/* [in] */ BSTR title,
	/* [in] */ IWebFrame *frame)
{
	BSTR frameName;
	if ( SUCCEEDED( frame->name( &frameName ) ) )
		m_Wrapper->OnReceiveTitle ( ToString (title), ToString (frameName) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WebViewDelegate::didFailProvisionalLoadWithError( 
	/* [in] */ IWebView *webView,
	/* [in] */ IWebError *error,
	/* [in] */ IWebFrame *frame)
{
	BSTR errorString;
	BSTR url;

	if (m_ExpectError)
	{
		m_ExpectError = false;
		return S_OK;
	}

	if ( SUCCEEDED( error->localizedDescription( &errorString ) ) && SUCCEEDED( error->failingURL( &url )) )
		ErrorString((ToString(errorString)+" url: "+ToString(url)).c_str());
	BSTR frameName;
	if ( SUCCEEDED( frame->name( &frameName ) ) )
		m_Wrapper->OnLoadError ( ToString (frameName) );
	return S_OK;
}
		
HRESULT STDMETHODCALLTYPE WebViewDelegate::didFailLoadWithError( 
	/* [in] */ IWebView *webView,
	/* [in] */ IWebError *error,
	/* [in] */ IWebFrame *frame)
{
	BSTR errorString;
	BSTR url;
	if ( SUCCEEDED( error->localizedDescription( &errorString ) ) && SUCCEEDED( error->failingURL( &url )) )
		ErrorString((ToString(errorString)+" url: "+ToString(url)).c_str());
	BSTR frameName;
	if ( SUCCEEDED( frame->name( &frameName ) ) )
		m_Wrapper->OnLoadError ( ToString (frameName) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WebViewDelegate::decidePolicyForNewWindowAction( 
	/* [in] */ IWebView *webView,
	/* [in] */ IPropertyBag *actionInformation,
	/* [in] */ IWebURLRequest *request,
	/* [in] */ BSTR frameName,
	/* [in] */ IWebPolicyDecisionListener *listener)
{
	BSTR url;

	listener->ignore();

	if ( SUCCEEDED ( request->URL( &url) ) )
		m_Wrapper->OnOpenExternalLink( ToString(url), ToString(frameName));

	return S_OK;
}


HRESULT STDMETHODCALLTYPE WebViewDelegate::webViewAddMessageToConsole( 
	/* [in] */ IWebView *sender,
	/* [in] */ BSTR message,
	/* [in] */ int lineNumber,
	/* [in] */ BSTR url,
	/* [in] */ BOOL isError)
{
	DebugStringToFile (ToString(message).c_str(), 0,  ToString(url).c_str(), lineNumber, isError?kError:kLog );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WebViewDelegate::webViewPainted( 
	/* [in] */ IWebView *sender)
{
	m_Dirty = true;
	m_Wrapper->OnWebViewDirty ();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WebViewDelegate::webViewDidInvalidate( 
	/* [in] */ IWebView *sender)
{
	m_Dirty = true;
	m_Wrapper->OnWebViewDirty ();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WebViewDelegate::webViewSetCursor( 
	/* [in] */ IWebView *sender,
	/* [in] */ OLE_HANDLE cursor)
{ 
	static HCURSOR* cursorHandles = NULL;
	if (cursorHandles == NULL)
	{
		cursorHandles = new HCURSOR[5];
		cursorHandles[GUIView::kArrow] = LoadCursor(NULL, IDC_ARROW);
		cursorHandles[GUIView::kText] = LoadCursor(NULL, IDC_IBEAM);
		cursorHandles[GUIView::kResizeVertical] = LoadCursor(NULL, IDC_SIZENS);
		cursorHandles[GUIView::kResizeHorizontal] = LoadCursor(NULL, IDC_SIZEWE);
		cursorHandles[GUIView::kLink] = LoadCursor(NULL, IDC_HAND);
		// TODO: More cursors will probbly make sense in the future
	}
	m_Dirty = true;

	HCURSOR handle = (HCURSOR)LongToHandle(cursor);
	for ( int i = 0; i < 5 ; ++i) 
	{
		if ( cursorHandles[i] == handle ) 
		{
			m_Wrapper->SetCursor(i);
			m_Wrapper->OnWebViewDirty ();

			return S_OK;
		}
	}
	m_Wrapper->OnWebViewDirty ();

	return S_OK;
}

HRESULT STDMETHODCALLTYPE WebViewDelegate::didReceiveResponse( 
	/* [in] */ IWebView *webView,
	/* [in] */ unsigned long identifier,
	/* [in] */ IWebURLResponse *response,
	/* [in] */ IWebDataSource *dataSource)
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

	// Work around a bug in webkit.
	// If the MIME type is NULL webkit will crash when trying to finish processing the loaded webpage. The crash happens
	// when certain errors occur (503 or 404 for example) but the MIME type is also unset in other situations (non-errors) where a
	// crash will not happen (like 304), so there we don't apply the fix. Here we just trigger the loading of the offline page, 
	// this will cancel the processing of the previous page load and thus the crash is circumvented and we appropriately 
	// show the offline store instead. We need to skip error handling in the next didFailProvisionalLoadWithError call 
	// as it's just reporting that the previous page load was cancelled and we don't want to trigger loading the offline store again.
	if (mimeType == NULL)
	{
		int status = 0;
		IWebHTTPURLResponse* httpResponse;
		if (SUCCEEDED(response->QueryInterface(&httpResponse)))
			httpResponse->statusCode( &status);
		if (status >= 400 && status < 600)
		{
			m_ExpectError = true;
			m_Wrapper->OnLoadError(ToString (frameName));
		}
	}

error:
	if ( frame )
		frame->Release ();
	return hr;
}

HRESULT STDMETHODCALLTYPE WebViewDelegate::didFinishLoadForFrame(IWebView *webView, IWebFrame *frame)
{
	if (!IsHumanControllingUs())
	{
		BSTR url;
		webView->mainFrameURL(&url);
		LogString(Format("Finished loading: %s\n", ToString(url).c_str()));
	}
	return S_OK;
}
#endif // ENABLE_ASSET_STORE
