#include "UnityPrefix.h"
#include "WebViewBaseDelegate.h"
#include "PlatformDependent/Win/WinUnicode.h"
#include "Runtime/Utilities/LogAssert.h"

WebViewBaseDelegate::WebViewBaseDelegate()
	: m_refCount(1)
{
}

WebViewBaseDelegate::~WebViewBaseDelegate()
{
}

HRESULT STDMETHODCALLTYPE WebViewBaseDelegate::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegate))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegate))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    //else if (IsEqualGUID(riid, IID_IWebUIDelegate2))
    //    *ppvObject = static_cast<IWebUIDelegate2*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegatePrivate))
        *ppvObject = static_cast<IWebUIDelegatePrivate*>(this);
	else if (IsEqualGUID(riid, IID_IWebPolicyDelegate))
		*ppvObject = static_cast<IWebPolicyDelegate*>(this);
	else if (IsEqualGUID(riid, IID_IWebResourceLoadDelegate)) 
		*ppvObject = static_cast<IWebResourceLoadDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}
ULONG STDMETHODCALLTYPE WebViewBaseDelegate::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebViewBaseDelegate::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

