#import <Cocoa/Cocoa.h>

@class WebView;

@interface LicenseWebViewWindow : NSWindow
{
	float m_Progress;
	NSWindow* m_ActualKeyWindow;
	BOOL m_ExpectError;	
	NSTimer* m_Timer;
}

- (LicenseWebViewWindow*) initWithContentRect:(NSRect) rect showResizeHandle: (BOOL)resizeable webView:(WebView*) view;

- (void)windowWillClose:(NSNotification *)notification;

- (void)LicenseTimer;

- (void)PageLoadStarted:(NSNotification*)theNotification;
- (void)PageLoadFinished:(NSNotification*)theNotification;

@end
