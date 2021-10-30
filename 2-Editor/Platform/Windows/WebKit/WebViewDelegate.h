#pragma once

#if ENABLE_ASSET_STORE

#include "Editor/Src/WebViewWrapper.h"
#include "Editor/Platform/Windows/WebKit/WebViewBaseDelegate.h"
#include <atlbase.h>

class WebViewDelegate : public WebViewBaseDelegate
{
private:
	WebViewWrapper* m_Wrapper;
	bool m_Dirty;
	bool m_ExpectError;
public:
	WebViewDelegate(WebViewWrapper* w);
	~WebViewDelegate();

	bool IsDirty()
	{
		return m_Dirty;
	}
	
	void SetDirty(bool value)
	{
		m_Dirty=value;
	}

    // IWebUIDelegate
	virtual HRESULT STDMETHODCALLTYPE webViewSetCursor( 
		/* [in] */ IWebView *sender,
		/* [in] */ OLE_HANDLE cursor);    
    
	// IWebFrameLoadDelegate

    virtual HRESULT STDMETHODCALLTYPE didReceiveTitle( 
        /* [in] */ IWebView *webView,
        /* [in] */ BSTR title,
        /* [in] */ IWebFrame *frame);

	virtual HRESULT STDMETHODCALLTYPE didFailProvisionalLoadWithError( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebError *error,
		/* [in] */ IWebFrame *frame);
		
	virtual HRESULT STDMETHODCALLTYPE didFailLoadWithError( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebError *error,
		/* [in] */ IWebFrame *forFrame);

	// IWebPolicyDelegate
    virtual HRESULT STDMETHODCALLTYPE decidePolicyForNewWindowAction( 
        /* [in] */ IWebView *webView,
        /* [in] */ IPropertyBag *actionInformation,
        /* [in] */ IWebURLRequest *request,
        /* [in] */ BSTR frameName,
        /* [in] */ IWebPolicyDecisionListener *listener);
    
	// IWebResourceLoadDelegate
	virtual HRESULT STDMETHODCALLTYPE didReceiveResponse( 
		/* [in] */ IWebView *webView,
		/* [in] */ unsigned long identifier,
		/* [in] */ IWebURLResponse *response,
		/* [in] */ IWebDataSource *dataSource);

	virtual HRESULT STDMETHODCALLTYPE didFinishLoadForFrame( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebFrame *frame);

protected:
    // IWebUIDelegatePrivate
    
    virtual HRESULT STDMETHODCALLTYPE webViewAddMessageToConsole( 
        /* [in] */ IWebView *sender,
        /* [in] */ BSTR message,
        /* [in] */ int lineNumber,
        /* [in] */ BSTR url,
        /* [in] */ BOOL isError);
    

    virtual HRESULT STDMETHODCALLTYPE doDragDrop( 
        /* [in] */ IWebView *sender,
        /* [in] */ IDataObject *dataObject,
        /* [in] */ IDropSource *dropSource,
        /* [in] */ DWORD okEffect,
        /* [retval][out] */ DWORD *performedEffect) { return E_NOTIMPL; /* TODO */ }

    virtual HRESULT STDMETHODCALLTYPE webViewPainted( 
        /* [in] */ IWebView *sender) ;

	virtual HRESULT STDMETHODCALLTYPE webViewDidInvalidate( 
            /* [in] */ IWebView *sender);


};

#endif // ENABLE_ASSET_STORE
