//
//  WebHTMLViewHacks.mm
//
//  Category that overrides important functions inside WebHTMLView either to add more hooks into WebKit or to avoid crashes.
//
#include "UnityPrefix.h"

#include "WebHTMLViewHacks.h"
#include "WrappedWebViewWindow.h"


namespace WebCore {
    class KeyboardEvent;
}

struct WebHTMLViewInterpretKeyEventsParameters;


@implementation WebHTMLView (unityHacks)

// Used to track which areas of the web view need to be grabbed and uploaded to the graphics card
- (void)setNeedsDisplayInRect:(NSRect)invalidRect
{
	NSWindow* win = [self window];
	if ( win && [win isKindOfClass: [WrappedWebViewWindow class]] )
		[(WrappedWebViewWindow*)win addDirtyRect: invalidRect];
	[super setNeedsDisplayInRect:invalidRect];
}

// This disables initiating a drag and drop operation from the web view, which causes Unity to crash for some reason
- (void)dragImage:(NSImage *)anImage at:(NSPoint)imageLoc offset:(NSSize)mouseOffset event:(NSEvent *)theEvent pasteboard:(NSPasteboard *)pboard source:(id)sourceObject slideBack:(BOOL)slideBack
{
	return;
}

// Since our web view pretends to have focus, the event chain will send some events to 
// performKeyEquivalent: . This can cause things like arrow key events being taken away 
// from other windows. Since we handle key combinations ourselves in WebViewWrapper 
// anyways, we can safely disable this by retuning NO, so events get passed into the
// normal event chain.
- (BOOL)performKeyEquivalent:(NSEvent *)event
{
	return NO;
}

#if 1 
- (void)mouseMovedNotification:(NSNotification *)notification
{
  // simply ignore global MouseMove events
//	[super mouseMovedNotification:notification];
}
#endif

- (void)windowDidBecomeKey:(NSNotification *)notification
{

}

- (void)windowDidResignKey:(NSNotification *)notification
{
	// We pretend we're alwas key window
}

@end

@implementation NSEvent (unityHacks)

+ (NSEvent *)scrollEventWithLocation:(NSPoint)location modifierFlags:(unsigned int)flags timestamp:(NSTimeInterval)time windowNumber:(int)windowNum context:(NSGraphicsContext *)context eventNumber:(int)eventNumber
 deltaX:(float)deltaX deltaY:(float)deltaY
{
	NSEvent* res = [NSEvent mouseEventWithType:(NSEventType)NSLeftMouseDown location:location 
		modifierFlags:flags timestamp:time windowNumber: windowNum context:context eventNumber: eventNumber clickCount:1 pressure:nil];
	res->_type = NSScrollWheel;
	res->_data.scrollWheel.deltaX=deltaX;
	res->_data.scrollWheel.deltaY=deltaY;
	res->_data.scrollWheel.deltaZ=0.0;
	return res;
}

- (void) setWindow: (NSWindow*) window
{
	_window=window;
	_windowNumber=[window windowNumber];
}

@end
