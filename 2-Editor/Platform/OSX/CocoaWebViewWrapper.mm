#include "UnityPrefix.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/BitUtility.h"
#include "EditorApplication.h"
#include "Configuration/UnityConfigureVersion.h"

#include "Editor/Src/WebViewWrapper.h"
#include "Editor/Src/WebViewScripting.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/Utility/CurlRequest.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/GfxDevice/GfxDevice.h"

#if ENABLE_ASSET_STORE && WEBVIEW_IMPL_WEBKIT

WebKeyboardEvent WebViewWrapper::GetCurrentKeyboardEvent()
{
	return [NSApp currentEvent];
}


WebViewWrapper::WebViewWrapper(int width, int height, bool showResizeHandle) : 
m_Width(width), 
m_Height(height), 
m_WebView(NULL),
m_Window(NULL),
m_TargetTexture(NULL),
m_WebFrame(NULL)
{
	m_WebView = [[WebView alloc] initWithFrame: NSMakeRect(0, 0, width, height) frameName: nil groupName: nil]  ;
	[m_WebView setApplicationNameForUserAgent: @"Unity/" UNITY_VERSION " (http://unity3d.com)"];
	//[(id)m_WebView setDrawsBackground: NO]; 
	[m_WebView setMaintainsBackForwardList:NO];
	
	// When you have make a view backed by a CA layer you need to draw/render the view through the layer
	// so for example NSBitmapImageRep::initWithFocusedViewRect will no longer work
	// We need this layer so we can render into it using a CGBitmapContext, that bypasses scaling (like on retina displays)
	[m_WebView setWantsLayer:YES];
	
	m_Window = [[WrappedWebViewWindow alloc] initWithContentRect: NSMakeRect(0,0, width, height) showResizeHandle: showResizeHandle webView: m_WebView wrapper: this] ;
	m_WebFrame = [m_WebView mainFrame] ;
	[[m_WebFrame frameView] setAllowsScrolling: YES];

	InitTexture( width, height );
	// [m_Window orderFront:m_Window]; // Uncomment to debug differences between hidden window and grabbed contents
 }

WebViewWrapper::~WebViewWrapper() 
{
	AssertIf(!Thread::CurrentThreadIsMainThread());
	if (m_WebView != NULL)
	{
		[m_WebView stopLoading:m_Window ];
		[m_Window close];
		[m_WebView release ];
	}
	m_WebFrame=NULL;
	m_WebView=NULL;
	m_Window=NULL;
	DestroyCommon ();
}


const NSEventType kMouseButtonRemap[] = 
{ 
	NSLeftMouseDown,	NSRightMouseDown,		NSOtherMouseDown,	// +0
	NSLeftMouseUp,		NSRightMouseUp,			NSOtherMouseUp,		// +3
	NSLeftMouseDragged,	NSRightMouseDragged,	NSOtherMouseDragged	// +6 
}; 

void InjectMouseEvent( int type, int x, int y, NSWindow *win, int clickCount ) 
{
	NSEvent* mouseEvent = [NSEvent mouseEventWithType:(NSEventType)type location:NSMakePoint(x, y) 
		modifierFlags:nil timestamp:GetCurrentEventTime() windowNumber: [win windowNumber] context:nil eventNumber: nil clickCount:clickCount pressure:nil];
	[mouseEvent setWindow: win];
	[win sendEvent: mouseEvent];
}

void WebViewWrapper::InjectMouseDown(int x, int y, int button, int clickCount) 
{
	InjectMouseEvent(kMouseButtonRemap[button], x, m_Height-y, m_Window, clickCount);
}
void WebViewWrapper::InjectMouseUp(int x, int y, int button, int clickCount) 
{
	InjectMouseEvent(kMouseButtonRemap[button+3], x, m_Height-y, m_Window, clickCount);
}

void WebViewWrapper::InjectMouseMove(int x, int y) 
{
	InjectMouseEvent(NSMouseMoved, x, m_Height-y, m_Window, 0);
}

void WebViewWrapper::InjectMouseDrag(int x, int y, int button) 
{
	InjectMouseEvent(kMouseButtonRemap[button+6], x, m_Height-y, m_Window, 0);	
}

void WebViewWrapper::Undo ()
{
	[[m_WebView undoManager] undo];
}

void WebViewWrapper::Redo ()
{
	[[m_WebView undoManager] redo];
}

bool WebViewWrapper::HasUndo ()
{
	return [[m_WebView undoManager] canUndo];
}

bool WebViewWrapper::HasRedo ()
{
	return [[m_WebView undoManager] canRedo];
}

void WebViewWrapper::SelectAll ()
{
	[m_WebView selectAll: nil];
}

void WebViewWrapper::Copy ()
{
	[m_WebView copy: nil];
}

void WebViewWrapper::Cut ()
{
	[m_WebView cut: nil];
}

void WebViewWrapper::Paste ()
{
	[m_WebView paste: nil];
}

void WebViewWrapper::InjectMouseWheel(int x, int y, float deltaX, float deltaY) 
{
	NSEvent* scrollEvent = [NSEvent scrollEventWithLocation:NSMakePoint(x, m_Height-y) modifierFlags:nil timestamp:GetCurrentEventTime() windowNumber:[ m_Window windowNumber] context:nil
		eventNumber:nil deltaX:deltaX	deltaY:-deltaY];
	NSView* subView= [m_WebView hitTest: [scrollEvent locationInWindow]];
	[subView scrollWheel:scrollEvent];
}

void ProcessArrowKeySelector (WebView *view, SEL selector)
{
	if (selector != nil)
	{
		if ([view respondsToSelector: selector])
			[view performSelector: selector withObject: nil];
	}
}

void ProcessArrowKeyEvent (WebView *view, NSEvent *event, SEL character, SEL characterSel, SEL word, SEL wordSel, SEL document, SEL documentSel)
{
	if ([event modifierFlags] & NSCommandKeyMask)
	{
		if ([event modifierFlags] & NSShiftKeyMask)
			ProcessArrowKeySelector (view, documentSel);
		else
			ProcessArrowKeySelector (view, document);
	}
	else if ([event modifierFlags] & NSAlternateKeyMask)
	{
		if ([event modifierFlags] & NSShiftKeyMask)
			ProcessArrowKeySelector (view, wordSel);
		else
			ProcessArrowKeySelector (view, word);
	}
	else 
	{
		if ([event modifierFlags] & NSShiftKeyMask)
			ProcessArrowKeySelector (view, characterSel);
		else
			ProcessArrowKeySelector (view, character);
	}

}

bool ProcessArrowKeyEvents (WebView *view, NSEvent *event)
{
    NSString *s = [event charactersIgnoringModifiers];
    if ([event type] == NSKeyDown && [s length] != 0)
	{
		switch ([s characterAtIndex:0]) {
			case NSLeftArrowFunctionKey:
				ProcessArrowKeyEvent (view, event, 
					@selector(moveLeft:), @selector(moveLeftAndModifySelection:),
					@selector(moveWordBackward:), @selector(moveWordBackwardAndModifySelection:),
					@selector(moveToBeginningOfLine:), @selector(moveToBeginningOfLineAndModifySelection:)
				);
				return true;
			case NSRightArrowFunctionKey:
				ProcessArrowKeyEvent (view, event, 
					@selector(moveRight:), @selector(moveRightAndModifySelection:),
					@selector(moveWordForward:), @selector(moveWordForwardAndModifySelection:),
					@selector(moveToEndOfLine:), @selector(moveToEndOfLineAndModifySelection:)
				);
				return true;
			case NSUpArrowFunctionKey:
				ProcessArrowKeyEvent (view, event, 
					@selector(moveUp:), @selector(moveUpAndModifySelection:),
					nil, nil,
					@selector(moveToBeginningOfDocument:), @selector(moveToBeginningOfDocumentAndModifySelection:)
				);
				return true;
			case NSDownArrowFunctionKey:
				ProcessArrowKeyEvent (view, event, 
					@selector(moveDown:), @selector(moveDownAndModifySelection:),
					nil, nil,
					@selector(moveToEndOfDocument:), @selector(moveToEndOfDocumentAndModifySelection:)
				);
				return true;
		}
	}
	return false;
}

void WebViewWrapper::InjectKeyboardEvent(const WebKeyboardEvent& keyboardEvent) 
{
	NSEvent* event = (NSEvent*) keyboardEvent ;
	[event setWindow: m_Window];
	
	// since our event handling sends two input events for the same Cocoa event
	// (one for character, one for key code, to match windows behavior), make sure
	// we are not processing the same event twice.
	static NSEvent* lastEvent = NULL;
	if (lastEvent == event)
		return;

	if (ProcessArrowKeyEvents (m_WebView, event))
	{
		lastEvent = event;
		return;
	}


	[m_Window sendEvent:event];
	lastEvent = event;

	/*
	NSLog(@"%@",event);
	switch ([event type] ) {
		case NSKeyDown:
		[[m_WebFrame frameView ] keyDown:event];
		break;
		case NSKeyUp:
		[[m_WebFrame frameView ] keyUp:event];
		case NSFlagsChanged:
		[[m_WebFrame frameView ] flagsChanged:event];
		break;
		default:
		NSLog(@"OTHER!");
		break;
	}*/
}

void WebViewWrapper::PostInitTexture(int width, int height)
{
	DoResizeImpl(width, height, true);
}

void WebViewWrapper::SetWebkitControlsGlobalLocation(int x, int y)
{
	NSArray* screens = [NSScreen screens];
	NSScreen* mainScreen = [screens objectAtIndex:0];
	NSRect screenFrame = [mainScreen frame];
	
	NSPoint origin;
	origin.x = x;
	origin.y = screenFrame.size.height - y;
	[ m_Window setFrameTopLeftPoint:origin ];
}

void WebViewWrapper::LoadURL(const string& url)
{
	[m_WebFrame  loadRequest:
	[NSURLRequest requestWithURL: [NSURL URLWithString: [NSString stringWithUTF8String: url.c_str()] ] ]];

}

void WebViewWrapper::LoadFile(const string& path)
{
	[m_WebFrame  loadRequest:
	[NSURLRequest requestWithURL: [NSURL fileURLWithPath: [NSString stringWithUTF8String: path.c_str()] ]]];
}

bool WebViewWrapper::IsDirty()
{
	return [m_Window webViewDirty];
}

int GetTextureImageFormat(int samplesPerPixel) {
	if(samplesPerPixel == 4)
		return kTexFormatRGBA32;
	else if(samplesPerPixel == 3)
		return kTexFormatRGB24;
	return 0; // unsupported
}


void WebViewWrapper::DoRenderImpl( ) 
{
	NSRect rect = [m_Window getDirtyRect];

	if (rect.size.width <= 0 || rect.size.height <= 0) // If unsure render everything
		rect = [m_WebView bounds];
	
	if (rect.size.height > m_Height)
		rect.size.height = m_Height;
	
	rect.origin.y = m_Height - rect.origin.y - rect.size.height;
	
	CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB(); 
	CGContextRef ctx = CGBitmapContextCreate(nil, rect.size.width, rect.size.height, 8, 4*(int)rect.size.width, colorSpaceRef, kCGImageAlphaPremultipliedLast);
	// Move the context origin to the portion of the view which is dirty
	CGContextTranslateCTM(ctx, -rect.origin.x, -rect.origin.y);
	[[m_WebView layer] renderInContext:ctx];
	
	unsigned char* bitmapData = (unsigned char*)CGBitmapContextGetData(ctx);
	
	int bitmapWidth = CGBitmapContextGetWidth(ctx);
	int bitmapHeight = CGBitmapContextGetHeight(ctx);
	int samplesPerPixel = CGBitmapContextGetBitsPerPixel(ctx)/8;
	
	int textureFormat = GetTextureImageFormat(samplesPerPixel);
	int dataSize = bitmapWidth * bitmapHeight * samplesPerPixel;
	int left = rect.origin.x;
	int top = m_TargetTexture->GetGLHeight() - rect.origin.y - rect.size.height;

	GetGfxDevice().UploadTextureSubData2D( m_TargetTexture->GetUnscaledTextureID(), (UInt8*) bitmapData, dataSize, 0,  left, top, bitmapWidth, bitmapHeight, textureFormat, m_TargetTexture->GetActiveTextureColorSpace());

	[m_Window setWebViewDirty: NO];
	CGColorSpaceRelease(colorSpaceRef);
	CGContextRelease(ctx);
}

void WebViewWrapper::DoResizeImpl( int width, int height, bool textureSizeChanged ) 
{
	[m_Window setContentSize: NSMakeSize(width, height) ];
	[m_WebView setFrameSize: NSMakeSize(width, height) ];
	[[NSNotificationCenter defaultCenter] postNotificationName: NSWindowDidResizeNotification	object: m_Window]; 

	[m_Window setWebViewDirty: YES];
	if ( textureSizeChanged )
		m_TargetTexture->UpdateImageDataDontTouchMipmap();

	UpdateTexture();
}

WebScriptObjectWrapper* WebViewWrapper::GetWindowScriptObject()
{
	return new WebScriptObjectWrapper([m_WebFrame globalContext ]);
}


#endif // ENABLE_ASSET_STORE && WEBVIEW_IMPL_WEBKIT
