#import <Cocoa/Cocoa.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebView.h>
#import <PlatformDependent/OSX/UnicodeInputView.h>

class WebViewWrapper;

@interface WrappedWebViewWindow : NSWindow
{
	WebViewWrapper* m_Wrapper;
	BOOL m_Dirty;
	float m_Progress;
	NSRect m_DirtyRect;
	NSWindow* m_ActualKeyWindow;
	UnicodeInputView *m_TextInputView;
	BOOL m_ExpectError;
	
	NSEvent* __fakeEvent;
	NSPoint __responderLocation;
}

- (WrappedWebViewWindow*) initWithContentRect:(NSRect) rect showResizeHandle: (BOOL)resizeable webView:(WebView*) view wrapper:(WebViewWrapper*) wrapper;


- (void)setWebViewDirty: (BOOL)value;
- (BOOL)webViewDirty;
- (void)PageLoadStarted:(NSNotification*)theNotification;
- (void)PageLoadFinished:(NSNotification*)theNotification;
- (void)addDirtyRect: (NSRect) dirty;
- (NSRect)getDirtyRect;

@end
