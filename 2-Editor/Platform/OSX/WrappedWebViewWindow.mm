#include "UnityPrefix.h"
#include "Runtime/Utilities/LogAssert.h"
#include "WrappedWebViewWindow.h"
#include "Editor/Src/WebViewWrapper.h"
#include "Runtime/Utilities/Argv.h"

static id __lastKeyDown = nil;

@implementation WrappedWebViewWindow

- (WrappedWebViewWindow*) initWithContentRect:(NSRect) rect showResizeHandle: (BOOL)resizeable webView:(WebView*) view wrapper:(WebViewWrapper*) wrapper
{
	m_Wrapper = wrapper;
	m_TextInputView = NULL;
	[self initWithContentRect: rect styleMask:resizeable?NSResizableWindowMask:0 backing: NSBackingStoreBuffered defer: NO];
	[self setBackgroundColor:[NSColor colorWithDeviceRed: 0.2 green: 0.2 blue: 0.2 alpha: 1.0]];
	[self setHasShadow:NO];

	[view setUIDelegate: self];
	[view setFrameLoadDelegate: self];
	[view setResourceLoadDelegate: self];
	[view setPolicyDelegate: self];
	
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(PageLoadStarted:) name:WebViewProgressStartedNotification object:view];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(PageLoadFinished:) name:WebViewProgressFinishedNotification object:view];

	[self setContentView: view];
	[self setInitialFirstResponder: view];
	
	return self;

}

- (void) dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self ];
	if (m_TextInputView)
	{
		[m_TextInputView setTextInputForwardingWindow: NULL];
		[m_TextInputView release];
	}

	[super dealloc];
}

- (NSEvent *)currentEvent {
	if( __fakeEvent != nil ) 
		return __fakeEvent;
	else
		return [super currentEvent];
}

- (void) makeKeyWindow
{									
	//if (_wFlags.isKeyWindow)
	//	return;
	m_ActualKeyWindow = [NSApp keyWindow]; // Save the current key window					
	[m_ActualKeyWindow resignKeyWindow];			
	[self becomeKeyWindow];
	_wFlags.isKeyWindow=YES;
}

- (void) makeKeyAndOrderFront:(id) sender
{
	//[self orderFront:sender];						// order self to the front
	_wFlags.visible=YES;
	//if(!_wFlags.isKeyWindow)
		[self makeKeyWindow];						// Make self the key window
}


// Reimplementation of event handling that pretends the window always is visible and accepting mouse events 
- (void) sendEvent:(NSEvent *)event
{
	if (__fakeEvent == event)
		return; // For some reason, key down events are passed in twice
	if (__fakeEvent) {
		[__fakeEvent release];
	}
	__fakeEvent = [ event retain ];
	
	if (!_wFlags.validCursorRects)
		[self resetCursorRects];

	UnicodeInputView *eventInputView = [UnicodeInputView getCurrentEventInputView];
	if (eventInputView != NULL)
	{
		[eventInputView setTextInputForwardingWindow: self];
		if (m_TextInputView != eventInputView)
		{
			if (m_TextInputView != NULL)
				[m_TextInputView release];
			m_TextInputView = eventInputView;
			[m_TextInputView retain];
		}
	}
	
	NSPoint l;
	NSView *v = NULL;
	
	switch ([event type])
	{
	case NSCursorUpdate:
	case NSAppKitDefined:
			[super sendEvent:event];
			break;
	case NSLeftMouseDown:								// Left mouse down
		_lastLeftHit = [[self contentView] hitTest:[event locationInWindow]];
		//[_lastLeftHit acceptsFirstMouse: event] ;
		
		if((NSResponder *) _lastLeftHit != _firstResponder && [(NSResponder *) _lastLeftHit acceptsFirstResponder]) {
			[self makeFirstResponder:_lastLeftHit];		// make hit view first responder if not already and if it accepts*/
		}
		//[self makeFirstResponder:[self contentView]];

		//ErrorString(string([[NSString stringWithFormat:@"%@", ll_lastLeftHit] UTF8String]));

		[_lastLeftHit mouseDown:event];

		
		break;

	case NSLeftMouseUp:									// Left mouse up
		[_lastLeftHit mouseUp:event];
		break;

	case NSRightMouseDown:								// Right mouse down
			
		_lastRightHit = [[self contentView] hitTest:[event locationInWindow]];
		[_lastRightHit rightMouseDown:event];
		break;

	case NSRightMouseUp:								// Right mouse up
		
		[_lastRightHit rightMouseUp:event];
		break;
	case NSOtherMouseDown:								// Middle mouse down
			
		_lastRightHit = [[self contentView] hitTest:[event locationInWindow]];
		[_lastRightHit otherMouseDown:event];
		break;

	case NSOtherMouseUp:								// Middle mouse up
		[[self contentView] reload: self];
		[_lastRightHit otherMouseUp:event];
		break;

	case NSMouseMoved:									// Mouse moved
		l = [event locationInWindow];
		v = [[self contentView] hitTest:[event locationInWindow]];
		[v mouseMoved:event];				// hit view passes event up
											// if we accept mouse moved
		if ([v respondsToSelector: @selector(_updateMouseoverWithEvent:)])
			[v performSelector: @selector(_updateMouseoverWithEvent:) withObject: event];
		[self mouseMoved:event];	// handle cursor
		break;

	case NSLeftMouseDragged:									// Mouse moved

		[_lastLeftHit mouseDragged:event];
		break;
		
	case NSRightMouseDragged:									// Mouse moved
		[_lastRightHit mouseDragged:event];
		break;
		
	case NSKeyDown:										// Key down
		{
		//if ( ! _wFlags.isKeyWindow )
			[self makeKeyAndOrderFront:self];
		// NSLog(@"%@",event);
		__lastKeyDown = _firstResponder;	// save the first responder so that the key up goes to it and not a possible new first responder
		[_firstResponder keyDown:event];
		break;
		}

	case NSKeyUp:
		if (__lastKeyDown)
			[__lastKeyDown keyUp:event];		// send Key Up to object that got the key down
		__lastKeyDown = nil;
		[self resignKeyWindow];			
		if( m_ActualKeyWindow )
		{
			[ m_ActualKeyWindow becomeKeyWindow ];
			m_ActualKeyWindow = nil;
		}
		break;

	case NSScrollWheel:
				
		[[[self contentView] hitTest:[event locationInWindow]] scrollWheel:event];
		break;

	default:
		break;
	}
	
	/*if (__fakeEvent != nil) {
		[__fakeEvent release];
		__fakeEvent = nil;
	}*/

}

- (void)setWebViewDirty: (BOOL)value
{
	m_Dirty = value;
	if (! m_Dirty )
	{
		m_DirtyRect = NSMakeRect(0, 0, 0, 0);
	}

}

- (BOOL)webViewDirty
{
	return m_Dirty;
}

- (void)addDirtyRect: (NSRect) dirty
{
	m_DirtyRect = NSUnionRect(m_DirtyRect, dirty);
	m_Dirty=( m_DirtyRect.size.width > 0 && m_DirtyRect.size.height > 0 );
	
	if ( m_Dirty ) 
		m_Wrapper->OnWebViewDirty();
}

- (NSRect)getDirtyRect
{
	return m_DirtyRect;
}

- (void)setViewsNeedDisplay:(BOOL) val
{
	[super setViewsNeedDisplay:val];
	if ( ! m_Dirty ) 
	{
		m_Dirty=true;
		m_Wrapper->OnWebViewDirty();
	}
}

- (BOOL)isKeyWindow
{
	return YES;// _wFlags.isKeyWindow;
}

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame
{
	m_Wrapper->OnReceiveTitle ( [title UTF8String], [[frame name] UTF8String] );
}

- (void)webView:(WebView *)sender resource:(id)identifier didReceiveResponse:(NSURLResponse *)response fromDataSource:(WebDataSource *)dataSource
{
	const char * url = [[[ response URL ] absoluteString] UTF8String];
	NSString* mimeType = [response MIMEType];
	const char * frameName = [[[ dataSource webFrame] name ] UTF8String];
	m_Wrapper->OnBeginLoading(url, frameName , [mimeType UTF8String]);
	
	if([response isKindOfClass:[NSHTTPURLResponse class]] )
	{
		NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
		int responseStatusCode = [httpResponse statusCode];
	
		// We received a http response of type text/html with no Content-Length and the response code indicates an internal server error (5xx), abort and load offline page 
		if ([response expectedContentLength] == -1 && [mimeType isEqualToString: @"text/html"] && responseStatusCode >= 500 && responseStatusCode < 600)
		{
			//LogString(Format("Error loading: %s\n", url))
			m_ExpectError = YES;
			m_Wrapper->OnLoadError(frameName);
		}
	}
}

- (void)webView:(WebView *)webView addMessageToConsole:(NSDictionary *)dictionary
{
	string message = [[dictionary objectForKey: @"message"] UTF8String];
	int t = kLog;
	if ( message.find("Error:") != string::npos )
		t = kError;
	DebugStringToFile (message, 0,  [[dictionary objectForKey: @"sourceURL"] UTF8String], [[dictionary objectForKey: @"lineNumber"] intValue], t);
}

- (void)PageLoadStarted:(NSNotification*)theNotification
{

}

- (void)PageLoadFinished:(NSNotification*)theNotification
{
	m_Wrapper->OnFinishLoading();

}

- (void)webView:(WebView *)sender decidePolicyForNewWindowAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request newFrameName:(NSString *)frameName decisionListener:(id < WebPolicyDecisionListener >)listener
{
	[listener ignore];
	m_Wrapper->OnOpenExternalLink([[[request URL] absoluteString ] UTF8String], [frameName UTF8String]);
		
}

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{	
	if (m_ExpectError)
		m_ExpectError = NO;
	else
		m_Wrapper->OnLoadError([[frame name] UTF8String]);
}  
  
- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
	m_Wrapper->OnLoadError([[frame name] UTF8String]);
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
	if (!IsHumanControllingUs())
		LogString(Format("Finished loading: %s\n", [[sender mainFrameURL] UTF8String]));
}
  
@end
