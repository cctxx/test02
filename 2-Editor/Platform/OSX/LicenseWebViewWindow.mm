#import "LicenseWebViewWindow.h"
#include "UnityPrefix.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Editor/Src/WebViewWrapper.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Src/LicenseInfo.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Editor/Src/Utility/CurlRequest.h"
#include "Editor/Src/LicenseActivationWindowCustomJS.h"

static LicenseWebViewWindow* s_WebViewWindow = NULL;

LicenseWebViewWindow* PrepareLicenseActivationWindow()
{
	NSRect initialRect = NSMakeRect(0, 0, 0, 0);
	WebView* webView = [[WebView alloc] initWithFrame: initialRect frameName: nil groupName: nil]  ;
	[webView setApplicationNameForUserAgent: @"Unity/" UNITY_VERSION " (http://unity3d.com)"];
	[webView setDrawsBackground: NO]; 
	[webView setMaintainsBackForwardList:NO];
	
	Assert(s_WebViewWindow == NULL);
	s_WebViewWindow = [[LicenseWebViewWindow alloc] initWithContentRect: initialRect showResizeHandle: YES webView: webView] ;
	[webView release];
	
	return s_WebViewWindow;
}

void LicenseInfo::CreateLicenseActivationWindow()
{
	if (s_WebViewWindow == NULL)
		PrepareLicenseActivationWindow();
	
	NSSize size;
	size.width = 640;
	size.height = 550;
	[s_WebViewWindow setContentSize: size];
	[s_WebViewWindow center];

	[s_WebViewWindow makeKeyAndOrderFront:nil];	
	
	NSString* url = [[[NSString alloc] initWithUTF8String:LicenseInfo::Get()->GetLicenseURL().c_str()] autorelease];
	WebView* webView = [s_WebViewWindow contentView];
	WebFrame* webFrame = [webView mainFrame] ;
	[[webFrame frameView] setAllowsScrolling: YES];
	LicenseLog("WebView opening %s\n", [url UTF8String]);
	[webFrame loadRequest: [NSURLRequest requestWithURL: [NSURL URLWithString: url]]];
	
	m_DidCreateWindow = true;
	if ([NSApp modalWindow] != s_WebViewWindow)
		[NSApp runModalForWindow:s_WebViewWindow];
}

void LicenseInfo::DestroyLicenseActivationWindow()
{
	if (s_WebViewWindow != NULL)
		[s_WebViewWindow close];
	
	m_DidCreateWindow = false;
}

void LicenseInfo::Relaunch(std::vector<std::string>& args)
{
	SetRelaunchApplicationArguments (args);
	//[NSApp terminate: [LicenseWebViewWindow class]];
	SetRelaunchApplicationArguments (vector<string> ());
}


@implementation LicenseWebViewWindow

- (LicenseWebViewWindow*) initWithContentRect:(NSRect) rect showResizeHandle: (BOOL)resizeable webView:(WebView*) view
{
	[self initWithContentRect: rect styleMask:(NSTitledWindowMask | NSClosableWindowMask | (resizeable?NSResizableWindowMask:0)) backing: NSBackingStoreBuffered defer: NO];
	[self setBackgroundColor:[NSColor colorWithDeviceRed: 0.2 green: 0.2 blue: 0.2 alpha: 1.0]];
	[self setHasShadow:NO];
     
	[view setUIDelegate: self];
	[view setFrameLoadDelegate: self];
	[view setResourceLoadDelegate: self];
	[view setPolicyDelegate: self];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowWillClose:) name:NSWindowWillCloseNotification object:self];
	
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(PageLoadStarted:) name:WebViewProgressStartedNotification object:view];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(PageLoadFinished:) name:WebViewProgressFinishedNotification object:view];
	
	[self setContentView: view];
	[self setInitialFirstResponder: view];
	
	m_Timer = [[NSTimer scheduledTimerWithTimeInterval: 0.1F 
		target: self selector:@selector(LicenseTimer) userInfo:nil repeats:true] retain];
	[[NSRunLoop currentRunLoop] addTimer:m_Timer forMode:NSModalPanelRunLoopMode];
	
	return self;	
}

- (void) sendEvent:(NSEvent *)event
{
	// Don't display context menu
	if ([event type] == NSRightMouseDown)
		return;
	[super sendEvent:event];
}

- (void) LicenseTimer
{
	CurlRequestCheck();
	LicenseInfo::Get()->Tick();
}

- (void) dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self ];
	
	s_WebViewWindow = NULL;
	
	[super dealloc];
}

- (void)webView:(WebView *)sender resource:(id)identifier didReceiveResponse:(NSURLResponse *)response fromDataSource:(WebDataSource *)dataSource
{
	NSString* mimeType = [response MIMEType];
	
	if([response isKindOfClass:[NSHTTPURLResponse class]] )
	{
		NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
		
		if( [mimeType isEqualToString:@"text/html"] && [response respondsToSelector:@selector(allHeaderFields)] )
		{
			NSDictionary * httpHeaders = [httpResponse allHeaderFields];
			LicenseLog("WebView received headers:\n%s\n", [[httpHeaders description] UTF8String]);
			
			NSInteger count = [httpHeaders count];
			id objects[count];
			id keys[count];
			[httpHeaders getObjects:objects andKeys:keys];            
			
			for (int i = 0; i < count; i++)
			{
				NSString* obj = objects[i];
				NSString* key = keys[i];
                
				if ([[key uppercaseString] isEqualToString: @"X-RX"])
					LicenseInfo::Get()->SetRXValue([obj UTF8String]);
			}
		}
	}
}


- (void)windowWillClose:(NSNotification *)aNotification
{
    LicenseInfo::Get()->SignalUserClosedWindow();
	[m_Timer invalidate];
	[m_Timer release];
	m_Timer = NULL;
	[NSApp stopModal];
}


- (void)PageLoadStarted:(NSNotification*)theNotification
{
	
}

- (void)PageLoadFinished:(NSNotification*)theNotification
{	

}

- (void)webView:(WebView *)webView addMessageToConsole:(NSDictionary *)dictionary
{
	string message = [[dictionary objectForKey: @"message"] UTF8String];
	int t = kLog;
	if ( message.find("Error:") != string::npos )
		t = kError;
	DebugStringToFile (message, 0,  [[dictionary objectForKey: @"sourceURL"] UTF8String], [[dictionary objectForKey: @"lineNumber"] intValue], t);
}

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{	
	LicenseLog("WebView provisional load error");
	
}  

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
	LicenseLog("WebView load error");
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
	JSGlobalContextRef globalContext = [frame globalContext];
	AddJSClasses(globalContext);
	
	LicenseLog("WebView finished loading: %s\n", [[sender mainFrameURL] UTF8String]);
}


- (void)webView:(WebView *)sender decidePolicyForNewWindowAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request newFrameName:(NSString *)frameName decisionListener:(id < WebPolicyDecisionListener >)listener
{
	[listener ignore];
    [[NSWorkspace sharedWorkspace] openURL:[request URL]];
}

@end