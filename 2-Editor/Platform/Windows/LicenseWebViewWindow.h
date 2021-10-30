#pragma once
#include "Editor/Src/WebViewWrapper.h"
#include "Editor/Platform/Windows/WebKit/WebViewBaseDelegate.h"

extern bool LicenseMessagePump();
extern void TurnOffSplashScreenWhenActivationWindowAppears();

class LicenseWebViewWindow : public WebViewBaseDelegate
{
private:
#if ENABLE_ASSET_STORE
	WebViewPtr m_WebView;
	WebFramePtr m_WebFrame;
#endif // ENABLE_ASSET_STORE
	HWND m_Window;
	bool m_windowVisible;
	int m_Width;
	int m_Height;
	bool m_ExpectError;

public:
	LicenseWebViewWindow();
	~LicenseWebViewWindow();

	void ExpectError();
	void HandleError(IWebView *webView, IWebError* error);

	// WebView delegates
	virtual HRESULT STDMETHODCALLTYPE webViewAddMessageToConsole(IWebView *sender, BSTR message, int lineNumber, BSTR url, BOOL isError);
	virtual HRESULT STDMETHODCALLTYPE didReceiveResponse(IWebView *webView, unsigned long identifier, IWebURLResponse *response, IWebDataSource *dataSource);
	virtual HRESULT STDMETHODCALLTYPE didFinishLoadForFrame(IWebView *webView, IWebFrame *frame);
	virtual HRESULT STDMETHODCALLTYPE didReceiveTitle(IWebView *webView, BSTR title, IWebFrame *frame);
	virtual HRESULT STDMETHODCALLTYPE decidePolicyForNewWindowAction(IWebView *webView, IPropertyBag *actionInformation, IWebURLRequest *request, BSTR frameName, IWebPolicyDecisionListener *listener);
	virtual HRESULT STDMETHODCALLTYPE didFailLoadingWithError(IWebView *webView, unsigned long identifier, IWebError *error, IWebDataSource *dataSource);
	virtual HRESULT STDMETHODCALLTYPE didFailLoadWithError(IWebView *webView, IWebError *error, IWebFrame *forFrame);
	virtual HRESULT STDMETHODCALLTYPE didFailProvisionalLoadWithError(IWebView *webView, IWebError *error, IWebFrame *frame);
};
