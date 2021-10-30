#pragma once

#include <atlbase.h>
#include <WebKit/WebKit.h>
#include <WebKit/WebKitCOMAPI.h>
#include <JavaScriptCore/JavaScriptCore.h>



// Dummy delegate class thar returns "not implemented" for all callback methods
// Used to make the definition of WebViewDelegate class cleaner
class WebViewBaseDelegate :
	public IWebUIDelegate, public IWebUIDelegatePrivate, 
	public IWebFrameLoadDelegate, public IWebPolicyDelegate, public IWebResourceLoadDelegate
{
private:
	ULONG m_refCount;
public:
	WebViewBaseDelegate();
	~WebViewBaseDelegate();
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
	virtual ULONG STDMETHODCALLTYPE AddRef(void);
	virtual ULONG STDMETHODCALLTYPE Release(void);

	// IWebUIDelegate
	virtual HRESULT STDMETHODCALLTYPE createWebViewWithRequest( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebURLRequest *request,
		/* [retval][out] */ IWebView **newWebView) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewShow( 
		/* [in] */ IWebView *sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewClose( 
		/* [in] */ IWebView *sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewFocus( 
		/* [in] */ IWebView *sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewUnfocus( 
		/* [in] */ IWebView *sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewFirstResponder( 
		/* [in] */ IWebView *sender,
		/* [retval][out] */ OLE_HANDLE *responder) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE makeFirstResponder( 
		/* [in] */ IWebView *sender,
		/* [in] */ OLE_HANDLE responder) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE setStatusText( 
		/* [in] */ IWebView *sender,
		/* [in] */ BSTR text) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewStatusText( 
		/* [in] */ IWebView *sender,
		/* [retval][out] */ BSTR *text) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewAreToolbarsVisible( 
		/* [in] */ IWebView *sender,
		/* [retval][out] */ BOOL *visible) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE setToolbarsVisible( 
		/* [in] */ IWebView *sender,
		/* [in] */ BOOL visible) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewIsStatusBarVisible( 
		/* [in] */ IWebView *sender,
		/* [retval][out] */ BOOL *visible) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE setStatusBarVisible( 
		/* [in] */ IWebView *sender,
		/* [in] */ BOOL visible) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewIsResizable( 
		/* [in] */ IWebView *sender,
		/* [retval][out] */ BOOL *resizable) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE setResizable( 
		/* [in] */ IWebView *sender,
		/* [in] */ BOOL resizable) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE setFrame( 
		/* [in] */ IWebView *sender,
		/* [in] */ RECT *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewFrame( 
		/* [in] */ IWebView *sender,
		/* [retval][out] */ RECT *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE setContentRect( 
		/* [in] */ IWebView *sender,
		/* [in] */ RECT *contentRect) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewContentRect( 
		/* [in] */ IWebView *sender,
		/* [retval][out] */ RECT *contentRect) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE runJavaScriptAlertPanelWithMessage( 
		/* [in] */ IWebView *sender,
		/* [in] */ BSTR message) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE runJavaScriptConfirmPanelWithMessage( 
		/* [in] */ IWebView *sender,
		/* [in] */ BSTR message,
		/* [retval][out] */ BOOL *result) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE runJavaScriptTextInputPanelWithPrompt( 
		/* [in] */ IWebView *sender,
		/* [in] */ BSTR message,
		/* [in] */ BSTR defaultText,
		/* [retval][out] */ BSTR *result) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE runBeforeUnloadConfirmPanelWithMessage( 
		/* [in] */ IWebView *sender,
		/* [in] */ BSTR message,
		/* [in] */ IWebFrame *initiatedByFrame,
		/* [retval][out] */ BOOL *result) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE runOpenPanelForFileButtonWithResultListener( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebOpenPanelResultListener *resultListener) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE mouseDidMoveOverElement( 
		/* [in] */ IWebView *sender,
		/* [in] */ IPropertyBag *elementInformation,
		/* [in] */ UINT modifierFlags) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE contextMenuItemsForElement( 
		/* [in] */ IWebView *sender,
		/* [in] */ IPropertyBag *element,
		/* [in] */ OLE_HANDLE defaultItems,
		/* [retval][out] */ OLE_HANDLE *resultMenu) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE validateUserInterfaceItem( 
		/* [in] */ IWebView *webView,
		/* [in] */ UINT itemCommandID,
		/* [in] */ BOOL defaultValidation,
		/* [retval][out] */ BOOL *isValid) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE shouldPerformAction( 
		/* [in] */ IWebView *webView,
		/* [in] */ UINT itemCommandID,
		/* [in] */ UINT sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE dragDestinationActionMaskForDraggingInfo( 
		/* [in] */ IWebView *webView,
		/* [in] */ IDataObject *draggingInfo,
		/* [retval][out] */ WebDragDestinationAction *action) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE willPerformDragDestinationAction( 
		/* [in] */ IWebView *webView,
		/* [in] */ WebDragDestinationAction action,
		/* [in] */ IDataObject *draggingInfo) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE dragSourceActionMaskForPoint( 
		/* [in] */ IWebView *webView,
		/* [in] */ LPPOINT point,
		/* [retval][out] */ WebDragSourceAction *action) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE willPerformDragSourceAction( 
		/* [in] */ IWebView *webView,
		/* [in] */ WebDragSourceAction action,
		/* [in] */ LPPOINT point,
		/* [in] */ IDataObject *pasteboard,
		/* [retval][out] */ IDataObject **newPasteboard) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE contextMenuItemSelected( 
		/* [in] */ IWebView *sender,
		/* [in] */ void *item,
		/* [in] */ IPropertyBag *element) { return E_NOTIMPL; } 

	virtual HRESULT STDMETHODCALLTYPE hasCustomMenuImplementation( 
		/* [retval][out] */ BOOL *hasCustomMenus) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE trackCustomPopupMenu( 
		/* [in] */ IWebView *sender,
		/* [in] */ OLE_HANDLE menu,
		/* [in] */ LPPOINT point) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE measureCustomMenuItem( 
		/* [in] */ IWebView *sender,
		/* [in] */ void *measureItem) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE drawCustomMenuItem( 
		/* [in] */ IWebView *sender,
		/* [in] */ void *drawItem) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE addCustomMenuDrawingData( 
		/* [in] */ IWebView *sender,
		/* [in] */ OLE_HANDLE menu) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE cleanUpCustomMenuDrawingData( 
		/* [in] */ IWebView *sender,
		/* [in] */ OLE_HANDLE menu) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE canTakeFocus( 
		/* [in] */ IWebView *sender,
		/* [in] */ BOOL forward,
		/* [out] */ BOOL *result) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE takeFocus( 
		/* [in] */ IWebView *sender,
		/* [in] */ BOOL forward) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE registerUndoWithTarget( 
		/* [in] */ IWebUndoTarget *target,
		/* [in] */ BSTR actionName,
		/* [in] */ IUnknown *actionArg) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE removeAllActionsWithTarget( 
		/* [in] */ IWebUndoTarget *target) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE setActionTitle( 
		/* [in] */ BSTR actionTitle) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE undo() { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE redo() { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE canUndo( 
		/* [retval][out] */ BOOL *result) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE canRedo( 
		/* [retval][out] */ BOOL *result) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE printFrame( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE ftpDirectoryTemplatePath( 
		/* [in] */ IWebView *webView,
		/* [retval][out] */ BSTR *path) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewHeaderHeight( 
		/* [in] */ IWebView *webView,
		/* [retval][out] */ float *result) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewFooterHeight( 
		/* [in] */ IWebView *webView,
		/* [retval][out] */ float *result) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE drawHeaderInRect( 
		/* [in] */ IWebView *webView,
		/* [in] */ RECT *rect,
		/* [in] */ OLE_HANDLE drawingContext) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE drawFooterInRect( 
		/* [in] */ IWebView *webView,
		/* [in] */ RECT *rect,
		/* [in] */ OLE_HANDLE drawingContext,
		/* [in] */ UINT pageIndex,
		/* [in] */ UINT pageCount) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewPrintingMarginRect( 
		/* [in] */ IWebView *webView,
		/* [retval][out] */ RECT *rect) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE canRunModal( 
		/* [in] */ IWebView *webView,
		/* [retval][out] */ BOOL *canRunBoolean) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE createModalDialog( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebURLRequest *request,
		/* [retval][out] */ IWebView **newWebView) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE runModal( 
		/* [in] */ IWebView *webView) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE isMenuBarVisible( 
		/* [in] */ IWebView *webView,
		/* [retval][out] */ BOOL *visible) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE setMenuBarVisible( 
		/* [in] */ IWebView *webView,
		/* [in] */ BOOL visible) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE runDatabaseSizeLimitPrompt( 
		/* [in] */ IWebView *webView,
		/* [in] */ BSTR displayName,
		/* [in] */ IWebFrame *initiatedByFrame,
		/* [retval][out] */ BOOL *allowed) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE paintCustomScrollbar( 
		/* [in] */ IWebView *webView,
		/* [in] */ HDC hDC,
		/* [in] */ RECT rect,
		/* [in] */ WebScrollBarControlSize size,
		/* [in] */ WebScrollbarControlState state,
		/* [in] */ WebScrollbarControlPart pressedPart,
		/* [in] */ BOOL vertical,
		/* [in] */ float value,
		/* [in] */ float proportion,
		/* [in] */ WebScrollbarControlPartMask parts) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE paintCustomScrollCorner( 
		/* [in] */ IWebView *webView,
		/* [in] */ HDC hDC,
		/* [in] */ RECT rect) { return E_NOTIMPL; }

	// IWebFrameLoadDelegate
	virtual HRESULT STDMETHODCALLTYPE didStartProvisionalLoadForFrame( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didReceiveServerRedirectForProvisionalLoadForFrame( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didFailProvisionalLoadWithError( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebError *error,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didCommitLoadForFrame( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didReceiveTitle( 
		/* [in] */ IWebView *webView,
		/* [in] */ BSTR title,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didChangeIcons(
		/* [in] */ IWebView *webView,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didReceiveIcon( 
		/* [in] */ IWebView *webView,
		/* [in] */ OLE_HANDLE image,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; } 

	virtual HRESULT STDMETHODCALLTYPE didFinishLoadForFrame( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didFailLoadWithError( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebError *error,
		/* [in] */ IWebFrame *forFrame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didChangeLocationWithinPageForFrame( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; } 

	virtual HRESULT STDMETHODCALLTYPE willPerformClientRedirectToURL( 
		/* [in] */ IWebView *webView,
		/* [in] */ BSTR url,
		/* [in] */ double delaySeconds,
		/* [in] */ DATE fireDate,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didCancelClientRedirectForFrame( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE willCloseFrame( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE windowScriptObjectAvailable( 
		/* [in] */ IWebView *sender,
		/* [in] */ JSContextRef context,
		/* [in] */ JSObjectRef windowObject) { return E_NOTIMPL; }

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE didClearWindowObject( 
		/* [in] */ IWebView* webView,
		/* [in] */ JSContextRef context,
		/* [in] */ JSObjectRef windowObject,
		/* [in] */ IWebFrame* frame) { return E_NOTIMPL; }

	// IWebFrameLoadDelegatePrivate
	virtual HRESULT STDMETHODCALLTYPE didFinishDocumentLoadForFrame( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didFirstLayoutInFrame( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; } 

	virtual HRESULT STDMETHODCALLTYPE didHandleOnloadEventsForFrame( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didFirstVisuallyNonEmptyLayoutInFrame( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	// IWebFrameLoadDelegatePrivate2
	virtual HRESULT STDMETHODCALLTYPE didDisplayInsecureContent( 
		/* [in] */ IWebView *sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didRunInsecureContent( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebSecurityOrigin *origin) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didClearWindowObjectForFrameInScriptWorld(IWebView*, IWebFrame*, IWebScriptWorld*) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didPushStateWithinPageForFrame( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; } 

	virtual HRESULT STDMETHODCALLTYPE didReplaceStateWithinPageForFrame( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; } 

	virtual HRESULT STDMETHODCALLTYPE didPopStateWithinPageForFrame( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	// IWebPolicyDelegate
	virtual HRESULT STDMETHODCALLTYPE decidePolicyForNavigationAction( 
		/* [in] */ IWebView *webView,
		/* [in] */ IPropertyBag *actionInformation,
		/* [in] */ IWebURLRequest *request,
		/* [in] */ IWebFrame *frame,
		/* [in] */ IWebPolicyDecisionListener *listener) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE decidePolicyForNewWindowAction( 
		/* [in] */ IWebView *webView,
		/* [in] */ IPropertyBag *actionInformation,
		/* [in] */ IWebURLRequest *request,
		/* [in] */ BSTR frameName,
		/* [in] */ IWebPolicyDecisionListener *listener) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE decidePolicyForMIMEType( 
		/* [in] */ IWebView *webView,
		/* [in] */ BSTR type,
		/* [in] */ IWebURLRequest *request,
		/* [in] */ IWebFrame *frame,
		/* [in] */ IWebPolicyDecisionListener *listener){ return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE unableToImplementPolicyWithError( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebError *error,
		/* [in] */ IWebFrame *frame) { return E_NOTIMPL; }

	// IWebResourceLoadDelegate
	virtual HRESULT STDMETHODCALLTYPE identifierForInitialRequest( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebURLRequest *request,
		/* [in] */ IWebDataSource *dataSource,
		/* [in] */ unsigned long identifier) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE willSendRequest( 
		/* [in] */ IWebView *webView,
		/* [in] */ unsigned long identifier,
		/* [in] */ IWebURLRequest *request,
		/* [in] */ IWebURLResponse *redirectResponse,
		/* [in] */ IWebDataSource *dataSource,
		/* [retval][out] */ IWebURLRequest **newRequest) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didReceiveAuthenticationChallenge( 
		/* [in] */ IWebView *webView,
		/* [in] */ unsigned long identifier,
		/* [in] */ IWebURLAuthenticationChallenge *challenge,
		/* [in] */ IWebDataSource *dataSource) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didCancelAuthenticationChallenge( 
		/* [in] */ IWebView *webView,
		/* [in] */ unsigned long identifier,
		/* [in] */ IWebURLAuthenticationChallenge *challenge,
		/* [in] */ IWebDataSource *dataSource) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didReceiveResponse( 
		/* [in] */ IWebView *webView,
		/* [in] */ unsigned long identifier,
		/* [in] */ IWebURLResponse *response,
		/* [in] */ IWebDataSource *dataSource) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didReceiveContentLength( 
		/* [in] */ IWebView *webView,
		/* [in] */ unsigned long identifier,
		/* [in] */ UINT length,
		/* [in] */ IWebDataSource *dataSource) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didFinishLoadingFromDataSource( 
		/* [in] */ IWebView *webView,
		/* [in] */ unsigned long identifier,
		/* [in] */ IWebDataSource *dataSource) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE didFailLoadingWithError( 
		/* [in] */ IWebView *webView,
		/* [in] */ unsigned long identifier,
		/* [in] */ IWebError *error,
		/* [in] */ IWebDataSource *dataSource) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE plugInFailedWithError( 
		/* [in] */ IWebView *webView,
		/* [in] */ IWebError *error,
		/* [in] */ IWebDataSource *dataSource) { return E_NOTIMPL; }


protected:
	// IWebUIDelegatePrivate

	virtual HRESULT STDMETHODCALLTYPE unused1() { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE unused2() { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE unused3() { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewScrolled( 
		/* [in] */ IWebView *sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewAddMessageToConsole( 
		/* [in] */ IWebView *sender,
		/* [in] */ BSTR message,
		/* [in] */ int lineNumber,
		/* [in] */ BSTR url,
		/* [in] */ BOOL isError) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewShouldInterruptJavaScript( 
		/* [in] */ IWebView *sender,
		/* [retval][out] */ BOOL *result) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewReceivedFocus( 
		/* [in] */ IWebView *sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewLostFocus( 
		/* [in] */ IWebView *sender,
		/* [in] */ OLE_HANDLE loseFocusTo) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE doDragDrop( 
		/* [in] */ IWebView *sender,
		/* [in] */ IDataObject *dataObject,
		/* [in] */ IDropSource *dropSource,
		/* [in] */ DWORD okEffect,
		/* [retval][out] */ DWORD *performedEffect) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewGetDlgCode( 
		/* [in] */ IWebView *sender,
		/* [in] */ UINT keyCode,
		/* [retval][out] */ LONG_PTR *code) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewPainted( 
		/* [in] */ IWebView *sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE exceededDatabaseQuota( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebFrame *frame,
		/* [in] */ IWebSecurityOrigin *origin,
		/* [in] */ BSTR databaseIdentifier) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE embeddedViewWithArguments( 
		/* [in] */ IWebView *sender,
		/* [in] */ IWebFrame *frame,
		/* [in] */ IPropertyBag *arguments,
		/* [retval][out] */ IWebEmbeddedView **view) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewClosing( 
		/* [in] */ IWebView *sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewSetCursor( 
		/* [in] */ IWebView *sender,
		/* [in] */ OLE_HANDLE cursor) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE webViewDidInvalidate( 
		/* [in] */ IWebView *sender) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE desktopNotificationsDelegate(
		/* [out] */ IWebDesktopNotificationsDelegate** result) { return E_NOTIMPL; }



};
