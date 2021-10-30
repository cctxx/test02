#include "UnityPrefix.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Editor/Platform/Interface/RepaintController.h"
#include "Editor/Platform/OSX/Utility/CocoaEditorUtility.h"
#include "Editor/Src/MenuController.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Editor/Platform/Interface/DragAndDrop.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Camera/RenderManager.h"
#include "External/ShaderLab/Library/shaderlab.h"
#include "Editor/Platform/Interface/UndoPlatformDependent.h"
#include "Editor/Src/Application.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#include "Editor/Src/TooltipManager.h"
#include "Runtime/Profiler/ProfilerHistory.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Utilities/RecursionLimit.h"
#include "Runtime/Input/GetInput.h"
#include "Editor/Src/AuxWindowManager.h"
#include "Editor/Src/Undo/UndoManager.h"
#include "Runtime/IMGUI/IMGUIUtils.h"
#include "Runtime/IMGUI/GUIWindows.h"
#include "Runtime/BaseClasses/Cursor.h"
#include <stack>


using std::vector;

// All aux windows. Used to implement click-outside-window-to-close
static vector<GUIView *> gs_AuxWindows;


// All container windows
typedef std::set<ContainerWindow*> ContainerWindows;
static ContainerWindows s_ContainerWindows;

// The one that's currently being processed. This is stored inside event handling if we're nesting.
static GUIView *s_CurrentView = NULL;

#ifdef MAC_OS_X_VERSION_10_6
@interface ContainerWindowDelegate : NSObject <NSWindowDelegate>
#else

@interface ContainerWindowDelegate : NSObject
#endif
{
@public
	ContainerWindow *m_Window;
}

@end

@interface OTNoTitleWindow : NSWindow
{
	bool m_IsZoomed;
	NSRect m_NonZoomedPosition;
}
-(BOOL)AddToWindowList;

@end

@implementation OTNoTitleWindow
-(void)dealloc
{
	[super dealloc];
}

- (BOOL)isRestorable
{
	return NO;
}

//In Interface Builder we set CustomWindow to be the class for our window, so our own initializer is called here.
#ifdef MAC_OS_X_VERSION_10_6
- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
#else
- (id)initWithContentRect:(NSRect)contentRect styleMask:(int)styleMask backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
#endif
{
    //Call NSWindow's version of this function, but pass in the all-important value of NSBorderlessWindowMask
    //for the styleMask so that the window doesn't have a title bar
    NSWindow* result = [super initWithContentRect:contentRect styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];

    //and while we're at it, make sure the window has a shadow, which will automatically be the shape of our custom content.
    [result setHasShadow: YES];

	m_IsZoomed = false;

    return result;
}

-(BOOL)AddToWindowList
{
	if ([[self delegate]respondsToSelector: @selector (AddToWindowList)])
		[[self delegate] performSelector:@selector(AddToWindowList)];
	return NO;
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem
{
	if ([menuItem action] == @selector(performClose:))
	{
		ContainerWindowDelegate* delegate = (ContainerWindowDelegate*)[self delegate];
		if (delegate && delegate->m_Window->GetShowMode () != ContainerWindow::kShowMainWindow)
			return true;
		else
			return false;
	}
	else if ([menuItem action] == @selector(performMiniaturize:) || [menuItem action] == @selector(performZoom:))
		return true;
	else
		return false;
}
- (IBAction)performClose:(id)sender
{
	[self close];
}

- (IBAction)performMiniaturize:(id)sender
{
	[self miniaturize:sender];
}

- (IBAction)performZoom:(id)sender
{
	[self zoom:sender];
}

//Since our windows are set to be non-resizable (and Cocoa doesn't allow anything else without a title bar), we have to do our own zoom. Sigh.
- (IBAction)zoom:(id)sender
{
	NSRect targetPos;
	if(!m_IsZoomed)
	{
		m_NonZoomedPosition = [self frame];
		targetPos = [[self screen] visibleFrame];
	}
	else
		targetPos = m_NonZoomedPosition;
	m_IsZoomed = !m_IsZoomed;
	[self setFrame: targetPos display: NO];
}

- (BOOL)isZoomed
{
	return m_IsZoomed;
}

// Custom windows that use the NSBorderlessWindowMask can't become key by default.  Therefore, controls in such windows
// won't ever be enabled by default.  Thus, we override this method to change that.
- (BOOL) canBecomeKeyWindow {
    return YES;
}
- (BOOL) canBecomeMainWindow { return YES; }
- (BOOL) acceptsFirstResponder { return YES; }
- (BOOL) becomeFirstResponder { return YES; }
- (BOOL) resignFirstResponder { return YES; }
@end


@interface OTNoTitlePanel : NSPanel
-(BOOL)AddToWindowList;
@end

@implementation OTNoTitlePanel

- (BOOL)isRestorable
{
	return NO;
}

//In Interface Builder we set CustomWindow to be the class for our window, so our own initializer is called here.
#ifdef MAC_OS_X_VERSION_10_6
- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
#else
- (id)initWithContentRect:(NSRect)contentRect styleMask:(int)styleMask backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
#endif
{
    //Call NSWindow's version of this function, but pass in the all-important value of NSBorderlessWindowMask
    //for the styleMask so that the window doesn't have a title bar
    NSWindow* result = [super initWithContentRect:contentRect styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];

    //and while we're at it, make sure the window has a shadow, which will automatically be the shape of our custom content.
    [result setHasShadow: YES];
    return result;
}
-(void)dealloc
{
	[super dealloc];
}

-(BOOL)AddToWindowList
{
	if ([[self delegate]respondsToSelector: @selector (AddToWindowList)])
		[[self delegate] performSelector:@selector(AddToWindowList)];
	return NO;
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem
{
	if ([menuItem action] == @selector(performClose:))
	{
		ContainerWindowDelegate* delegate = (ContainerWindowDelegate*)[self delegate];
		if (delegate && delegate->m_Window->GetShowMode () != ContainerWindow::kShowMainWindow)
			return true;
		else
			return false;
	}
	else if ([menuItem action] == @selector(performMiniaturize:) || [menuItem action] == @selector(performZoom:))
		return true;
	else
		return false;
}
- (IBAction)performClose:(id)sender
{
	[self close];
}

- (IBAction)performMiniaturize:(id)sender
{
	[self miniaturize:sender];
}

- (IBAction)performZoom:(id)sender
{
	[self zoom:sender];
}

// Custom windows that use the NSBorderlessWindowMask can't become key by default.  Therefore, controls in such windows
// won't ever be enabled by default.  Thus, we override this method to change that.
- (BOOL) canBecomeKeyWindow {
    return YES;
}


@end

@implementation GUIOpenGLView

-(GraphicsContextHandle)context
{
	return m_Context;
}

- (id)initWithFrame:(NSRect) frameRect {
	self = [super initWithFrame:frameRect];
	if(!self)
		NSLog(@"initWithFrame failed");

	m_NeedsContextUpdate = true;
	m_LastFrame = frameRect;
	m_HasFocus = false;
//	[[NSNotificationCenter defaultCenter] addObserver:self
//			selector:@selector(FrameChangedNotification:)
//			name:NSViewBoundsDidChangeNotification object:self];

//	[[NSNotificationCenter defaultCenter] addObserver:self
//			selector:@selector(FrameChangedNotification:)
//			name:NSViewFrameDidChangeNotification object:self];

	[self registerForDraggedTypes: GetDragAndDrop().GetAllSupportedTypes()];

	return self;
}

- (void)dealloc {
	[[NSNotificationCenter defaultCenter]removeObserver: self];
	[super dealloc];
}

- (void)windowDidBecomeKey: (NSNotification *)aNotification {
	if (!m_HasFocus)
	{
		m_View->GotFocus ();
		m_HasFocus = true;
	}
}

- (void)windowDidResignKey: (NSNotification *)aNotification {
	if (m_HasFocus)
	{
		m_View->LostFocus ();
		m_HasFocus = false;
	}
}

- (BOOL)acceptsFirstResponder {
	if (!m_HasFocus)
		m_HasFocus = m_View->GotFocus();
	return m_HasFocus;
}


// acceptsFirstResponder is called before becomeFirstResponder. It's probably possibel to call GotFocus in only one of them.
// But i am not sure if cocoa always calls acceptsFirstResponder before becomeFirstResponder.
- (BOOL)becomeFirstResponder {
	if (!m_HasFocus)
		m_HasFocus = m_View->GotFocus();
	if (m_HasFocus)
		return [super becomeFirstResponder];
	else
		return false;
}

- (BOOL)resignFirstResponder {
	if (m_HasFocus)
	{
		m_View->LostFocus();
		m_HasFocus = false;
	}
	return [super resignFirstResponder];
}


// Forward all calls to the inputInterface
- (void)mouseDown:(NSEvent *)event {
	[self abandonMarkedText];
	GetApplication().LockReloadAssemblies();
	[self UpdateScreenManager];

	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[super mouseDown: event];
}
- (void)otherMouseDown:(NSEvent *)event {
	[self abandonMarkedText];
	GetApplication().LockReloadAssemblies();
	[self UpdateScreenManager];

	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[super otherMouseDown: event];
}
- (void)rightMouseDown:(NSEvent *)event {
	[self abandonMarkedText];
	GetApplication().LockReloadAssemblies();
	[self UpdateScreenManager];

	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[super rightMouseDown: event];
}
-(void)mouseDragged: (NSEvent *)event {
	[self UpdateScreenManager];
	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[super mouseDragged: event];
}
-(void)rightMouseDragged: (NSEvent *)event {
	[self UpdateScreenManager];
	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[super rightMouseDragged: event];
}
-(void)otherMouseDragged: (NSEvent *)event {
	[self UpdateScreenManager];
	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[super otherMouseDragged: event];
}
-(void)mouseUp: (NSEvent *)event {
	GetApplication().UnlockReloadAssemblies();
	[self UpdateScreenManager];
	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[super mouseUp: event];
}
-(void)rightMouseUp: (NSEvent *)event {
	GetApplication().UnlockReloadAssemblies();
	[self UpdateScreenManager];
	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[super rightMouseUp: event];
}
-(void)otherMouseUp: (NSEvent *)event {
	GetApplication().UnlockReloadAssemblies();
	[self UpdateScreenManager];
	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[super otherMouseUp: event];
}
- (void)scrollWheel:(NSEvent *)event {
	[self UpdateScreenManager];
	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[super scrollWheel: event];
}

- (void)keyDown:(NSEvent *)event {
	//Gameview gets cmd-key events higher up the responder chain, so it gets Cmd-C, etc. Just eat them here.
	if (m_View->IsGameView() != 0 && ([event modifierFlags] & NSCommandKeyMask))
		return;

	[self UpdateScreenManager];
	[super keyDown: event];
}

- (void)keyUp:(NSEvent *)event {
	//Gameview gets cmd-key events higher up the responder chain, so it gets Cmd-C, etc. Just eat them here.
	if (m_View->IsGameView() && ([event modifierFlags] & NSCommandKeyMask))
		return;

	[self UpdateScreenManager];
	[super keyUp: event];
}

- (BOOL)sendInputEvent: (InputEvent&)e {
	if (m_View->OnInputEvent (e))
		return YES;
	if (CallGlobalInputEvent(e))
		return YES;
	return NO;
}

- (void)mouseEnter:(NSEvent *)event {
}

- (void)mouseExit:(NSEvent *)event {
}

- (void)magnifyWithEvent:(NSEvent *)event
{
	[self UpdateScreenManager];
	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
	{
		if (!CallGlobalInputEvent(e))
			[[self nextResponder] performSelector:@selector(magnifyWithEvent:) withObject: event];
	}
}

- (void)rotateWithEvent:(NSEvent *)event
{
	[self UpdateScreenManager];
	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
	{
		if (!CallGlobalInputEvent(e))
			[[self nextResponder] performSelector:@selector(rotateWithEvent:) withObject: event];
	}
}

- (void)swipeWithEvent:(NSEvent *)event
{
	[self UpdateScreenManager];
	InputEvent e (event, self);
	if (!m_View->OnInputEvent (e))
		[[self nextResponder] performSelector:@selector(swipeWithEvent:) withObject: event];
}

- (NSMenu*)menuForEvent:(NSEvent*)event
{
	[self UpdateScreenManager];
	InputEvent e (event, self);
	e.type = InputEvent::kContextClick;
	m_View->OnInputEvent (e);

	if (HasDelayedContextMenu ())
	{
		GetApplication().ResetReloadAssemblies ();
		return ExtractDelayedContextMenu ();
	}
	else
		return NULL;
}

- (BOOL)wantsPeriodicDraggingUpdates
{
	return YES;
}

static NSDragOperation DragVisualModeToNS (DragAndDrop::DragVisualMode mode)
{
	if (mode == DragAndDrop::kDragOperationRejected)
		return DragAndDrop::kDragOperationNone;

	return mode;
}

- (int)doDrag:(id <NSDraggingInfo>)sender type: (InputEvent::Type)type
{
	GetUndoManager().RevertAllInCurrentGroup ();

	InputEvent event (sender, type, self);

	int oldVisualmode = GetDragAndDrop().GetVisualMode();
	GetDragAndDrop().Setup (sender);
	[self UpdateScreenManager];
	m_View->OnInputEvent (event);
	GetDragAndDrop().Cleanup ();

	if (type == InputEvent::kDragPerform)
	{
		GetUndoManager().IncrementCurrentGroup();
	}
	else if (type == InputEvent::kDragExited)
	{
		GetUndoManager().RevertAllInCurrentGroup ();
	}

	if (oldVisualmode != GetDragAndDrop().GetVisualMode())
		m_View->RequestRepaint();

	GetApplication().SetSceneRepaintDirty ();

	return DragVisualModeToNS(GetDragAndDrop().GetVisualMode());
}


- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	// When entering a drag we increment the event group
	// This ensures that the previous event group is not going to be undone by the
	// continous RevertAllInCurrentGroup.
	GetUndoManager().IncrementCurrentGroup();

	return [self draggingUpdated: sender];
}

- (void)draggingEnded:(id < NSDraggingInfo >)sender
{
	[self draggingExited: sender];
}

- (void)concludeDragOperation:(id < NSDraggingInfo >)sender
{
	[self draggingExited: sender];
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
	return [self doDrag: sender type: InputEvent::kDragUpdated];
}

- (void)draggingExited:(id < NSDraggingInfo >)sender
{
	[self doDrag: sender type:InputEvent::kDragExited];
	GetDragAndDrop().SetActiveControlID (0);
}
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
	// If the dragging updated returns false we will not even try to perform the drag
	if ([self draggingUpdated: sender] == NSDragOperationNone)
		return NO;

	[self doDrag: sender type: InputEvent::kDragPerform];

	GetDragAndDrop().SetActiveControlID (0);
	bool result = GetDragAndDrop().GetDragAccepted ();

	return result;
}

- (void)renderRect:(NSRect)frame {
	if (m_View)
	{
		// Set up window
		GetScreenManager().SetupScreenManagerEditor( frame.size.width, frame.size.height );
		m_View->ClearCursorRects ();
		// not sure this used to be frame.origin, but it certainly didn't work with the new EditorWindows (where we actually move the views around).
		GetRenderManager().SetWindowRect (Rectf (0, 0, frame.size.width, frame.size.height));
		// Clear back buffer
		GfxDevice& device = GetGfxDevice();
		device.DisableScissor();
		device.SetWireframe(false);

		//Never set srgb write for 'normal' windows
		device.SetSRGBWrite(false);

		// Feed input event
		bool oldTextFocus = GetInputManager().GetTextFieldInput();
		GetInputManager().SetTextFieldInput(false);
		InputEvent ie = InputEvent::RepaintEvent (self);
		m_View->OnInputEvent (ie);
		m_View->UpdateOSRects ();
		if(GetKeyGUIView() != m_View)
			GetInputManager().SetTextFieldInput(oldTextFocus);
	}
	else
	{
		ErrorString("No window setup yet");
	}
}

-(BOOL) isOpaque {
	return YES;
}

-(BOOL)acceptsFirstMouse:(NSEvent *)theEvent
{
	return true;
}

static map<string, NSCursor*> s_CursorMap;
static NSCursor* GetCursor (string imageName, int x, int y)
{
	NSCursor* cursor;

	map<string, NSCursor*>::iterator it = s_CursorMap.find (imageName);
	if (it != s_CursorMap.end ())
	{
		cursor = it->second;
	}
	else
	{
		cursor = [[NSCursor alloc] initWithImage: [NSImage imageNamed:[NSString stringWithUTF8String:imageName.c_str()]] hotSpot:NSMakePoint (x, y)];

		s_CursorMap.insert (pair<string, NSCursor*> (imageName, cursor));

		if (cursor == nil || cursor.image == nil)
			ErrorString (Format ("Failed To load cursor %s", imageName.c_str ()));//, [imagePath UTF8String]));
	}

	return cursor;
}

-(void)resetCursorRects
{
	for (vector<GUIView::CursorRect>::const_iterator i = m_View->m_CursorRects.begin(); i != m_View->m_CursorRects.end(); i++)
	{
		// Don't explicitly apply cursor rects for arrow cursors.
		// It is not needed, as the arrow cursor is the default cursor anyways,
		// and it interfers with WebKits cursor handling.
		if (i->mouseCursor != GUIView::kArrow)
		{
			NSCursor *cur = nil;
			// Source cursors from /System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/HIServices.framework/Versions/A/Resources/cursors
			switch (i->mouseCursor)
			{
				case GUIView::kArrow:
					cur = [NSCursor arrowCursor];
					break;
				case GUIView::kText:
					cur = [NSCursor IBeamCursor];
					break;
				case GUIView::kResizeHorizontal:
					cur = GetCursor ("ResizeHorizontal", 16, 16);
					break;
				case GUIView::kResizeVertical:
					cur = GetCursor ("ResizeVertical", 16, 16);
					break;
				case GUIView::kLink:
					cur = [NSCursor pointingHandCursor];
					break;
				case GUIView::kSlideArrow:
					cur = GetCursor ("SlideArrow", 9, 5);
					break;
				case GUIView::kResizeUpRight:
					cur = GetCursor ("ResizeUpRight", 16, 16);
					break;
				case GUIView::kResizeUpLeft:
					cur = GetCursor ("ResizeUpLeft", 16, 16);
					break;
				case GUIView::kMoveArrow:
					cur = GetCursor ("MoveArrow", 9, 5);
					break;
				case GUIView::kScaleArrow:
					cur = GetCursor ("ScaleArrow", 9, 5);
					break;
				case GUIView::kRotateArrow:
					cur = GetCursor ("RotateArrow", 9, 5);
					break;
				case GUIView::kArrowPlus:
					cur = GetCursor ("PlusArrow", 9, 5);
					break;
				case GUIView::kArrowMinus:
					cur = GetCursor ("MinusArrow", 9, 5);
					break;
				case GUIView::kPan:
					cur = [NSCursor openHandCursor];
					break;
				case GUIView::kOrbit:
					cur = GetCursor ("OrbitView", 16, 16);
					break;
				case GUIView::kZoom:
					cur = GetCursor ("ZoomView", 16, 16);
					break;
				case GUIView::kFPS:
					cur = GetCursor ("FPSView", 16, 16);
					break;
				case GUIView::kCustomCursor:
					cur = Cursors::GetHardwareCursor ();
					if (!cur)
						continue;
					break;
				case GUIView::kSplitResizeUpDown:
					cur = [NSCursor resizeUpDownCursor];
					break;
				case GUIView::kSplitResizeLeftRight:
					cur = [NSCursor resizeLeftRightCursor];
					break;
			}
			NSRect r = NSMakeRect (i->position.x, [self frame].size.height - i->position.y - i->position.height, i->position.width, i->position.height);
			[self addCursorRect:r cursor:cur];
		}
	}
}

@end

GUIView* GetKeyGUIView ()
{
	NSWindow* window = [NSApp keyWindow];
	NSResponder *view = [window firstResponder];
	if ([view isMemberOfClass: [GUIOpenGLView class]])
		return ((GUIOpenGLView*)view)->m_View;

	return NULL;
}

void BeginHandles ()
{
	if (MONO_COMMON.beginHandles)
	{
		GUIState& state = GetGUIState ();
		state.m_OnGUIDepth++;
		ScriptingInvocation(MONO_COMMON.beginHandles).Invoke();
		state.m_OnGUIDepth--;
	}
}

void EndHandles ()
{
	if (MONO_COMMON.endHandles)
	{
		GUIState& state = GetGUIState ();
		state.m_OnGUIDepth++;
		ScriptingInvocation(MONO_COMMON.endHandles).Invoke();
		state.m_OnGUIDepth--;
	}
}

static ContainerWindow* gMainWindow = NULL;
static ContainerWindow* gDraggingWindow = NULL;

#ifndef MAC_OS_X_VERSION_10_7
/* You may specify at most one of NSWindowCollectionBehaviorFullScreenPrimary or NSWindowCollectionBehaviorFullScreenAuxiliary. */
enum {
    NSWindowCollectionBehaviorFullScreenPrimary = 1 << 7,   // the frontmost window with this collection behavior will be the fullscreen window.
    NSWindowCollectionBehaviorFullScreenAuxiliary = 1 << 8,	  // windows with this collection behavior can be shown with the fullscreen window.
    NSApplicationPresentationFullScreen = (1 << 10)                    // Application is in fullscreen mode
};
#endif

void ContainerWindow::Init (MonoBehaviour* behaviour, Rectf pixelRect, int showMode, const Vector2f& minSize, const Vector2f& maxSize)
{
	m_ShowMode = static_cast<ShowMode>(showMode);
	m_InternalRect.x = m_InternalRect.y = m_InternalRect.width = m_InternalRect.height = 0.0f;
	m_WantsToResize = false;

	//Need to set this before setting up window, so the Mono Rect can be set, when window needs to be moved on screen.
	m_Instance = behaviour;

	NSRect rect = NSMakeRect (0,0,320,200);
	switch (showMode) {
	case kShowNormalWindow:
		m_Window = [[OTNoTitleWindow alloc] initWithContentRect: rect
			styleMask: /* NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask | */NSBorderlessWindowMask
			backing:NSBackingStoreBuffered defer:NO];
		// Set window to floating if we are in fullscreen mode
		if ([NSApp respondsToSelector:@selector(currentSystemPresentationOptions)])
		{
			if ([NSApp currentSystemPresentationOptions] & NSApplicationPresentationFullScreen)
				[m_Window setLevel:NSFloatingWindowLevel];
		}
		break;
	case kShowMainWindow:
		m_Window = [[OTNoTitleWindow alloc] initWithContentRect: rect
			styleMask: NSTitledWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
			backing:NSBackingStoreBuffered defer:NO];
		[m_Window setHidesOnDeactivate:NO];
		[m_Window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary];
		gMainWindow = this;
		GetApplication().UpdateMainWindowTitle ();
		GetApplication().UpdateDocumentEdited ();
		break;
	case kShowPopupMenu:
		m_Window = [[OTNoTitlePanel alloc] initWithContentRect: rect
			styleMask: NSBorderlessWindowMask
			backing:NSBackingStoreBuffered defer:NO];
		[m_Window setHasShadow: YES];
		[m_Window setHidesOnDeactivate:YES];
		[m_Window setLevel:NSPopUpMenuWindowLevel];
		break;
	case kShowNoShadow:
		m_Window = [[OTNoTitlePanel alloc] initWithContentRect: rect
			styleMask: 0
			backing:NSBackingStoreBuffered defer:NO];
		[m_Window setHasShadow: NO];
		[m_Window setHidesOnDeactivate:NO];
		[m_Window setLevel:NSFloatingWindowLevel];
		break;
	case kShowUtility:
		m_Window = [[OTNoTitlePanel alloc] initWithContentRect: rect
			// For some reason the NSResizableWindowMask flag causes weird behaviour when dragging to resize a window:
			// - Other windows won't redraw while dragging (which means Debug.Log messages appear all at once after dragging is done.)
			// - AnimValueManager won't update while dragging.
			// - Some other things behave weirdly too.
			// The window can apparently still be resized fine without setting this flag, and when it's not set, all the weirdness goes away.
			// Thus, let's try to just remove it. (Apparently, it was removed for the kShowNormalWindow as well...)
			styleMask:  NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | /*NSResizableWindowMask |*/ NSUtilityWindowMask
			backing:NSBackingStoreBuffered defer:NO];
		[m_Window setHasShadow: YES];
		[m_Window setHidesOnDeactivate:YES];
		[m_Window setLevel:kCGFloatingWindowLevel];
		break;
	case kShowAuxWindow:
		m_Window = [[OTNoTitlePanel alloc] initWithContentRect: rect
			styleMask:  NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask | NSUtilityWindowMask | (1 << 13)
			backing:NSBackingStoreBuffered defer:NO];
		[m_Window setHasShadow: YES];
		[m_Window setHidesOnDeactivate:YES];
		[m_Window setLevel:kCGFloatingWindowLevel];
		break;
	default:
		ErrorString("Unknown container show mode");
	}

	NSSize p;
	p.width = minSize.x;
	p.height = minSize.y;
	[m_Window setContentMinSize:p];
	p.width = maxSize.x;
	p.height = maxSize.y;
	[m_Window setContentMaxSize:p];

	[m_Window setReleasedWhenClosed: YES];
	[m_Window setAcceptsMouseMovedEvents: YES];
	[m_Window useOptimizedDrawing: YES];
	if (pixelRect.width > 10 || pixelRect.height > 10)
		SetRect(pixelRect);

	ContainerWindowDelegate* windowDelegate = [[ContainerWindowDelegate alloc]init];
	[m_Window setDelegate:windowDelegate];
	((ContainerWindowDelegate*) windowDelegate)->m_Window = this;
}

ContainerWindow::ContainerWindow ()
{
	m_Instance = NULL;
	m_Window = NULL;
	s_ContainerWindows.insert(this);
}

ContainerWindow::~ContainerWindow ()
{
	// Dont make this a full assert because
	// it locks up the persistent manager when doing threaded background loading
	DebugAssertIf(GetMonoContainerWindowData());
	[[m_Window delegate] autorelease];
	[m_Window setDelegate: NULL];


	if (gMainWindow == this)
		gMainWindow = NULL;
	if (gDraggingWindow == this)
		gDraggingWindow = NULL;

	s_ContainerWindows.erase(this);
}

void ContainerWindow::SetFreezeDisplay (bool freeze)
{
	if (freeze)
		NSDisableScreenUpdates();
	else
		NSEnableScreenUpdates();
}

void ContainerWindow::DisplayAllViews ()
{
	[m_Window display];
}

void ContainerWindow::BringLiveAfterCreation (bool immediately, bool setFocus)
{
	if (immediately)
	{
		if (m_ShowMode == kShowPopupMenu)
			[m_Window orderFront: NULL];
		else
			[m_Window makeKeyAndOrderFront: NULL];
	}
	else
	{
		if (m_ShowMode == kShowPopupMenu)
			[(ContainerWindowDelegate*)[m_Window delegate] performSelector: @selector(orderFrontDelayed) withObject: NULL afterDelay: 0.5f];
		else
			[(ContainerWindowDelegate*)[m_Window delegate] performSelector: @selector(makeKeyAndOrderFrontDelayed) withObject: NULL afterDelay: 0.0f];
	}
}


void ContainerWindow::SetAlpha (float alpha)
{
	[m_Window setAlphaValue:alpha];
	[m_Window setIgnoresMouseEvents:NO];
}

void ContainerWindow::SetInvisible ()
{
	[m_Window setAlphaValue:0.0f];
	[m_Window setIgnoresMouseEvents:NO];
}

Vector2f ContainerWindow::GetTopleftScreenPosition ()
{
	NSPoint viewTopLeft = NSMakePoint (0, NSHeight ([m_Window contentRectForFrameRect:[m_Window frame]])); //[view convertPoint: NSMakePoint (0, [view frame].size.height) toView: nil];
	NSPoint bottomLeft = [m_Window convertBaseToScreen: viewTopLeft];
	NSRect screenFrame = [[[NSScreen screens] objectAtIndex:0] frame];
	NSPoint topLeft = NSMakePoint (bottomLeft.x, screenFrame.size.height - bottomLeft.y);

	return Vector2f (topLeft.x, topLeft.y);
}


void ContainerWindow::SetHasShadow (bool hasShadow)
{
	[m_Window setHasShadow:hasShadow];
}

void ContainerWindow::SetTitle (const string &title)
{
	if (m_ShowMode != kShowMainWindow)
		[m_Window setTitle:[NSString stringWithUTF8String:title.c_str()]];
}

void ContainerWindow::SetMinMaxSizes (const Vector2f &minSize, const Vector2f &maxSize)
{
	NSSize p;
	p.width = minSize.x;
	p.height = minSize.y;
	[m_Window setContentMinSize:p];

	p.width = maxSize.x;
	p.height = maxSize.y;
	[m_Window setContentMaxSize:p];
}

void ContainerWindow::MakeModal()
{
	int disables = 0;
	while (!GetApplication().MayUpdate())
	{
		GetApplication().EnableUpdating(false);
		disables++;
	}
	DisplayAllViews ();
	[NSApp runModalForWindow: m_Window];
	while (disables > 0)
	{
		GetApplication().DisableUpdating();
		disables--;
	}

}

void ContainerWindow::MoveInFrontOf(ContainerWindow *other)
{
	if(other != NULL)
		[m_Window orderWindow:NSWindowAbove relativeTo: [other->m_Window windowNumber]];
	else
		[m_Window orderWindow:NSWindowAbove relativeTo: 0];
}

void ContainerWindow::MoveBehindOf(ContainerWindow *other)
{
	if(other != NULL)
		[m_Window orderWindow:NSWindowBelow relativeTo: [other->m_Window windowNumber]];
	else
		[m_Window orderWindow:NSWindowBelow relativeTo: 0];
}


enum VisibleMode {
	kDontForceVisible,
	kForceVisible,
	kForceCompletelyVisible
};

NSRect FitFrame (NSRect aFrame, NSScreen* screen, VisibleMode forceVisible)
{
	const int kMenuBarHeight = 22;
	const int kTitleBarHeight = 20;

	NSArray *screens = [NSScreen screens];
	NSRect mainScreenFrame = [[screens objectAtIndex:0] frame];
	bool isOnMainScreen = NSIntersectsRect(mainScreenFrame, aFrame);
	NSRect screenRect;
	if(screen)
		screenRect = [screen visibleFrame];
	else
	{
		screenRect = NSMakeRect (0,0,0,0);
		for (int i=0;i<[screens count ];i++)
		{
			id screen = [screens objectAtIndex:i];
			screenRect = NSUnionRect (screenRect, [screen visibleFrame]);
		}

		bool clipAgainstMenuBarAndDock = isOnMainScreen;

		int dockHeight = [[[NSScreen screens] objectAtIndex:0] visibleFrame].origin.y;
		if(dockHeight > 0 && clipAgainstMenuBarAndDock)
		{
			screenRect.size.height -= dockHeight - screenRect.origin.y;
			screenRect.origin.y = dockHeight;
		}

		//There is a screen above the main screen. Don't clip against menu bar, unless it covers the title bar then.
		if (NSMaxY(screenRect) > NSMaxY(mainScreenFrame))
		{
			if(NSMaxY(aFrame) > NSMaxY(mainScreenFrame) + kTitleBarHeight)
				clipAgainstMenuBarAndDock = false;
		}

		if(clipAgainstMenuBarAndDock)
			screenRect.size.height = mainScreenFrame.size.height - kMenuBarHeight - screenRect.origin.y;
	}

	// clamp height
	if (NSHeight(aFrame) > NSHeight(screenRect))
	{
		float tem = NSMaxY (aFrame);
		aFrame.size.height = screenRect.size.height;
		aFrame.origin.y = tem - aFrame.size.height;
	}
	// clamp width
	if (NSWidth(aFrame) > NSWidth(screenRect))
	{
		aFrame.size.width = screenRect.size.width;
	}

	if(forceVisible != kDontForceVisible)
	{
		// Move window down if it's too high. In this case make sure entire window is below menubar, so it can be dragged
		if (NSMaxY(aFrame) > NSMaxY(screenRect))
			aFrame.origin.y = floor(NSMaxY(screenRect) - NSHeight(aFrame));

		if(forceVisible == kForceCompletelyVisible)
		{
			if (NSMinY(aFrame) < NSMinY(screenRect))
				aFrame.origin.y = ceil(NSMinY(screenRect));
			if (NSMaxX(aFrame) > NSMaxX(screenRect))
				aFrame.origin.x = floor(NSMaxX(screenRect) - NSWidth(aFrame));
			else if (NSMinX(aFrame) < NSMinX(screenRect))
				aFrame.origin.x = ceil(NSMinX(screenRect));
		}
		else
		{
			if (NSMaxY(aFrame) < NSMinY(screenRect) + kTitleBarHeight)
				aFrame.origin.y = ceil(NSMinY(screenRect)) - aFrame.size.height + kTitleBarHeight;
			if (NSMinX(aFrame) > NSMaxX(screenRect))
				aFrame.origin.x = floor(NSMaxX(screenRect) - NSWidth(aFrame));
			else if (NSMaxX(aFrame) < NSMinX(screenRect))
				aFrame.origin.x = ceil(NSMinX(screenRect));

			//If we have been moved onto the mainscreen, call FitFrame again, because now we have to take into account the menu bar.
			if(isOnMainScreen != NSIntersectsRect(mainScreenFrame, aFrame))
				aFrame = FitFrame (aFrame, screen, forceVisible);
		}
	}
	return aFrame;
}

//Unfortunately Apple officially doesn't provide any API for using spaces.
//Since we are handling dragging of windows ourselves, we need to handle moving windows around spaces ourselves as well, unfortunately.
//These are the internal functions used in Apple's ApplicationServices framework. They are undocumented and subject to change
typedef int (*fp_CGSDefaultConnection) ( void );
typedef OSStatus (*fpCGSGetWorkspace) ( const int cid, int *workspace );
typedef OSStatus (*fpCGSGetWindowWorkspace) ( const int cid, const int wid, int *workspace);
typedef OSStatus (*fpCGSMoveWorkspaceWindowList) ( const int cid, const int *wids, int count, int workspace);

//We can't hardlink these functions, since we use 10.4 API. So, get them dynamically
fp_CGSDefaultConnection _CGSDefaultConnection = NULL;
fpCGSGetWorkspace CGSGetWorkspace = NULL;
fpCGSGetWindowWorkspace CGSGetWindowWorkspace = NULL;
fpCGSMoveWorkspaceWindowList CGSMoveWorkspaceWindowList = NULL;
CFBundleRef LoadSystemBundle(const char* frameworkName);

static bool InitWorkspaces()
{
	if (CGSGetWorkspace == NULL)
	{
		CFBundleRef qtBundle = LoadSystemBundle("ApplicationServices.framework");

		_CGSDefaultConnection=(fp_CGSDefaultConnection)CFBundleGetFunctionPointerForName (qtBundle, CFSTR("_CGSDefaultConnection"));
		if(!_CGSDefaultConnection)
			return false;
		CGSGetWorkspace=(fpCGSGetWorkspace)CFBundleGetFunctionPointerForName (qtBundle, CFSTR("CGSGetWorkspace"));
		if(!CGSGetWorkspace)
			return false;
		CGSGetWindowWorkspace=(fpCGSGetWindowWorkspace)CFBundleGetFunctionPointerForName (qtBundle, CFSTR("CGSGetWindowWorkspace"));
		if(!CGSGetWindowWorkspace)
			return false;
		CGSMoveWorkspaceWindowList=(fpCGSMoveWorkspaceWindowList)CFBundleGetFunctionPointerForName (qtBundle, CFSTR("CGSMoveWorkspaceWindowList"));
		if(!CGSMoveWorkspaceWindowList)
			return false;
	}
	return true;
}

//Query if user switched spaces. This needs to be queried, there is no notfication for this
void ContainerWindow::HandleSpacesSwitch()
{
	static int currentSpace = -1;
	if (InitWorkspaces())
	{
		int screenWorkspace;
		CGSGetWorkspace(_CGSDefaultConnection(), &screenWorkspace);

		//If the user switches spaces, while dragging a window, update window position.
		if(currentSpace != screenWorkspace && gDraggingWindow)
		{
			gDraggingWindow->SetRect(gDraggingWindow->GetRect());
			currentSpace = screenWorkspace;
		}
	}
}

void ContainerWindow::SetIsDragging(bool isDragging)
{
	if(isDragging)
		gDraggingWindow = this;
	else
		gDraggingWindow = NULL;
}

bool ContainerWindow::IsZoomed()
{
	return [m_Window isZoomed];
}

void ContainerWindow::SetRect (const Rectf &rect)
{
	if(!IMGUI::AreWeInOnGUI(GetGUIState()))
		DoSetRect(rect);
	else
	{
		m_NewSize = rect;
		m_WantsToResize = true;
	}
}

void ContainerWindow::DoSetRect (const Rectf &newRect)
{
	Rectf rect = newRect;
	// Make sure the window is on the pixel grid (OSX doesn't guarantee this if dragging while zooming, and we can also place them from code)
	rect.width = Roundf (rect.width);
	rect.height = Roundf (rect.height);
	rect.x = Roundf (rect.x);
	rect.y = Roundf (rect.y);

	if(m_ShowMode == kShowPopupMenu)
		//Popups (Tooltips) should be completely covered by the current screen
		rect = FitWindowRectToScreen (rect, true, true);

	// Convert to cocoa-space
	NSRect screenFrame = [[[NSScreen screens] objectAtIndex:0] frame];
	NSRect newSize = NSMakeRect (rect.x, screenFrame.size.height - rect.y - rect.height, rect.width, rect.height);
	NSRect r = [m_Window frameRectForContentRect: newSize];

	if (NSEqualRects(r, [[m_Window screen] frame]))
	{
		// Mac OS X 10.8 seems to have serious application focus and redraw issues when switching applications,
		// if a window frame matches the size of a screen exactly (this probably does not happen with other apps,
		// as normally, windows have title bars). Probably it triggers some code meant only for fullscreen apps.
		// To avoid this, we make sure that the window frame is one pixel smaller then the screen size.
		// Reported to Apple as Apple Bug ID# 12005027
		r.size.height -= 1;
		r.origin.y += 1;
	}

	m_WantsToResize = false;
	[m_Window setFrame: r display:YES animate:NO];

	if (gDraggingWindow == this && InitWorkspaces())
	{
		// if this window is on a different space then the current, and we are dragging it, move it to the current space.
		// This lets users drag windows from one spaces to another by dragging and switching spaces (normally supported
		// automatically, but we have to do it ourselves, since we use custom windows). Only do this for the current
		// dragging window so user assigned application spaces are respected.
		int cid = _CGSDefaultConnection();
		int wid = [m_Window windowNumber];
		int screenWorkspace, windowWorkspace;
		CGSGetWorkspace(cid, &screenWorkspace);
		CGSGetWindowWorkspace(cid, wid, &windowWorkspace);
		if(windowWorkspace && windowWorkspace != screenWorkspace)
		{
			CGSMoveWorkspaceWindowList(cid, &wid, 1, screenWorkspace);

			//when we are dragging a window from one space to another, we have to make sure it is in front ourselves.
			[m_Window makeKeyAndOrderFront: NULL];
			[[NSApplication sharedApplication] activateIgnoringOtherApps: YES];
		}
	}

	m_InternalRect = GetRect();

	MonoContainerWindowData* data = GetMonoContainerWindowData();
	if( data )
		data->m_PixelRect = newRect;

	CallMethod ("OnResize");

	// Update the scene so that scripts marked with [ExecuteInEditMode] are able to react to screen size changes
	GetApplication().SetSceneRepaintDirty ();
}

Rectf ContainerWindow::GetRect () const
{
	NSRect r = [m_Window contentRectForFrameRect: [m_Window frame]];
	NSRect screenFrame = [[[NSScreen screens] objectAtIndex:0] frame];

	return Rectf (r.origin.x, NSHeight (screenFrame) - r.origin.y - r.size.height, r.size.width, r.size.height);
}

void ContainerWindow::OnRectChanged()
{
	Rectf rect = GetRect();
	if( ![m_Window isVisible] || rect == m_InternalRect )
		return;

	// We got crash reports indicating endless recursive loops here.
	// Since we don't have steps to reproduce, we just cut off the recursion instead.
	LIMIT_RECURSION (5,);

	//To make sure window is on screen.
	SetRect (rect);
}

void ContainerWindow::PerformSizeChanges()
{
	ContainerWindows::iterator it, itEnd = s_ContainerWindows.end();
	for( it = s_ContainerWindows.begin(); it != itEnd; ++it ) {
		ContainerWindow* window = *it;
		if(window->m_WantsToResize)
			window->DoSetRect(window->m_NewSize);
	}
}

void ContainerWindow::Close()
{
	[m_Window close];
}

void ContainerWindow::Minimize ()
{
	[m_Window miniaturize:nil];
}

void ContainerWindow::ToggleMaximize ()
{
	[m_Window zoom:nil];
}


@implementation ContainerWindowDelegate
- (void)dealloc
{
	[super dealloc];
}

- (void)windowDidEnterFullScreen:(NSNotification *)aNotification
{
	// Set all normal windows to floating when we enter fullscreen.
	for (ContainerWindows::iterator it = s_ContainerWindows.begin(); it != s_ContainerWindows.end(); it++ )
	{
		ContainerWindow* window = *it;
		if (window->GetShowMode() == ContainerWindow::kShowNormalWindow)
			[window->m_Window setLevel:NSFloatingWindowLevel];
	}
}

- (void)windowDidExitFullScreen:(NSNotification *)aNotification
{
	// Disable floating for all normal windows when we exit fullscreen.
	for (ContainerWindows::iterator it = s_ContainerWindows.begin(); it != s_ContainerWindows.end(); it++ )
	{
		ContainerWindow* window = *it;
		if (window->GetShowMode() == ContainerWindow::kShowNormalWindow)
			[window->m_Window setLevel:NSNormalWindowLevel];
	}
}

- (void)windowWillClose:(NSNotification *)notif
{
	// When executed from mouse down we never get the mouse up!
	GetApplication().ResetReloadAssemblies();

	MonoContainerWindowData* data = m_Window->GetMonoContainerWindowData();

	if ([NSApp modalWindow] == m_Window->m_Window)
		[NSApp stopModal];

	data->m_WindowPtr = NULL;

	m_Window->CallMethod ("InternalCloseWindow");

	delete m_Window;
	m_Window = NULL;
}

- (void)windowDidMove:(NSNotification *)aNotification
{
	if (m_Window)
		m_Window->OnRectChanged();
}

- (void)windowDidResize:(NSNotification *)aNotification
{
	if (m_Window)
		m_Window->OnRectChanged();
}

// Called when the window gets key focus
- (void)windowDidBecomeKey: (NSNotification *)aNotification
{
	/// Give the dragtab a chance to notice that windows are reordered.
	CallStaticMonoMethod ("EditorApplication", "Internal_CallWindowsReordered");

	id v = [[aNotification object] firstResponder];
	if ([v respondsToSelector: @selector(windowDidBecomeKey:)])
		[v windowDidBecomeKey: aNotification];
}

- (void)windowDidResignKey: (NSNotification *)aNotification
{
	id v = [[aNotification object] firstResponder];
	if ([v respondsToSelector: @selector(windowDidResignKey:)])
		[v windowDidResignKey: aNotification ];
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)sender
{
	return GetGlobalCocoaUndoManager();
}

-(void)AddToWindowList
{
	if (m_Window)
		m_Window->CallMethod ("AddToWindowList");
}

-(void)makeKeyAndOrderFrontDelayed
{
	if (m_Window)
		[m_Window->m_Window makeKeyAndOrderFront: NULL];
}

-(void)orderFrontDelayed
{
	if (m_Window)
		[m_Window->m_Window orderFront: NULL];
}



@end

// TODO: Rewrite to not that delegate shit
void ContainerWindow::GetOrderedWindowList ()
{
	int windowCount;
	NSCountWindows(&windowCount);
	int windowList[windowCount];
	NSWindowList(windowCount, windowList);

	SEL sel = @selector(AddToWindowList);

	for (int i = 0; i < windowCount; i++) {
		NSWindow *win = [NSApp windowWithWindowNumber:windowList[i]];
		if (win && [win respondsToSelector:sel])
			[win performSelector: sel];
	}
}

typedef UNITY_SET(kMemEditorGui,GUIView*) GUIViews;
extern GUIViews g_GUIViews;


GUIView* GUIView::GetCurrent ()
{
	return s_CurrentView;
}

MonoBehaviour* GUIView::GetCurrentMonoView ()
{
	if (s_CurrentView)
		return s_CurrentView->m_Instance;
	else
		return NULL;
}

void GUIView::SendLayoutEvent (GUIState &state)
{
	InputEvent::Type originalType = state.m_CurrentEvent->type;
	state.m_CurrentEvent->type = InputEvent::kLayout;
	BeginHandles ();
	m_Instance->DoGUI(MonoBehaviour::kEditorWindowLayout, 1);
	EndHandles ();
	state.m_CurrentEvent->type = originalType;
}

// Returns true if the event was used/grabbed/eaten
bool GUIView::OnInputEvent (InputEvent &event)
{
	if (event.type == InputEvent::kIgnore)
		return false;

	// Check if GUIView has already been destroyed...
	if (g_GUIViews.find (this) == g_GUIViews.end () || !m_Instance.IsValid())
		return false;

	if (event.type == InputEvent::kMouseDown)
	{
		bool wasMouseDownStolen = GetAuxWindowManager().OnMouseDown (this);
		if (wasMouseDownStolen)
			return true;
	}

	// In debug mode, check that we're not leaking GUIDepth.
#if DEBUGMODE
	int eventType = event.type;
	int guiDepth = GetGUIState().m_OnGUIDepth;
#endif

	// If we're being called from a nested OnGUI call,
	GUIState* tempState = NULL;
	GUIView* oldCurrentView = NULL;
	if (GetGUIState().m_OnGUIDepth > 0)
	{
		tempState = GUIState::GetPushState();
		oldCurrentView = s_CurrentView;
	}
	s_CurrentView = this;


	HandleAutomaticUndoGrouping (event);

	m_KeyboardState.m_ShowKeyboardControl = GetKeyGUIView() == this;

	InputEvent::Type originalType = event.type;
	GUIState &state = BeginGUIState (event);

	SendLayoutEvent (state);

	int handleTab = 0;
	if (event.type == InputEvent::kKeyDown && (event.character == '\t' || event.character == 25))
		handleTab = ((event.modifiers & InputEvent::kShift) == 0) ? 1 : -1;

	// DoGUI will crash if the view has gone so check here!
	if (s_CurrentView == NULL) // we might be deleted by now!
		return false;

	BeginHandles ();

	// Mousedown should unfocus any windows. (if it's inside the window, it'll get refocused straight away which is cool :)
	if (event.type == InputEvent::kMouseDown)
		IMGUI::FocusWindow (state, -1);

	[UnicodeInputView setCurrentEventInputView: m_View];
	bool result = m_Instance->DoGUI(MonoBehaviour::kEditorWindowLayout, 1);
	[UnicodeInputView setCurrentEventInputView: NULL];

	EndHandles ();

	bool currentViewGone = (s_CurrentView == NULL); // we might be deleted by now!

	if (handleTab != 0)
	{
		// Build the list of IDLists to cycle through
		std::vector<IDList*> keyIDLists;
		IMGUI::GUIWindow* focusedWindow = IMGUI::GetFocusedWindow (state);
		if (focusedWindow)
			keyIDLists.push_back (&focusedWindow->m_ObjectGUIState.m_IDList);
		else
			keyIDLists.push_back (&m_Instance->GetObjectGUIState().m_IDList);

		state.CycleKeyboardFocus(keyIDLists, handleTab == 1);
		event.Use ();
		result = true;
	}

	// If the event was used, we want to repaint.
	// Do this before starting drags or showing menus, as they might delete this view!
	if (result && !currentViewGone)
		RequestRepaint ();

	if (!currentViewGone)
		EndGUIState (state);



	/// Delay start any drags. This is to avoid reentrant behaviour on UnityGUI,
	/// because drags in cocoa seem to create a loop inside of the dragImage function
	GetDragAndDrop().ApplyQueuedStartDrag();

	// Display context menu if script code generated one.
	// Except when we are dealing with a context click, then we let it through and return the context menu directly to cocoa.
	// This isnecessary because cocoa otherwise randomly decides to not bring up the context menu. Seems like a bug in cocoa.
	// Also don't do this for repaint events, to match Windows implementation.
	if (originalType != InputEvent::kContextClick && originalType != InputEvent::kRepaint)
	{
		if (HasDelayedContextMenu ())
		{
			// Cocoa will not send a mouse up when the modal context menu is complete.
			GetApplication().ResetReloadAssemblies ();
			ShowDelayedContextMenu ();
			// After showing context menu, repaint this view if it still exists
			if( g_GUIViews.find(this) == g_GUIViews.end() )
				currentViewGone = true;
			if (!currentViewGone)
				RequestRepaint();
		}
	}

	EndHandles ();
	s_CurrentView = oldCurrentView;

	// Send mouse down event to TooltipManager
	if (event.type == InputEvent::kMouseDown || event.type == InputEvent::kMouseMove)
	{
		GetTooltipManager().SendEvent(event);
	}

	ContainerWindow::PerformSizeChanges();

	// If this is a modal window the main application loop won't send repaint events.
	// Enforce repainting here.
	if (!currentViewGone && m_NeedsRepaint && event.type != InputEvent::kRepaint && event.type != InputEvent::kLayout)
	{
		if ([NSApp modalWindow] == [m_View window])
		{
			ForceRepaint();
			m_NeedsRepaint = false;
		}
	}

	if (tempState)
		GUIState::PopAndDelete (tempState);

	s_CurrentView = oldCurrentView;

	if (s_CurrentView == NULL)
	{
		// After done with top-level event processing of this view, set screen parameters to the game view
		bool gameHasFocus;
		Rectf gameRect, gameCameraRect;
		GetScreenParamsFromGameView(true, false, &gameHasFocus, &gameRect, &gameCameraRect);
	}

#if DEBUGMODE
	if (GetGUIState().m_OnGUIDepth != guiDepth)
		ErrorStringObject (Format ("OnGUIDepth changed: was %d is %d. Event type was %d", guiDepth, GetGUIState().m_OnGUIDepth, eventType), m_Instance);
#endif

	
	HandleAutomaticUndoOnEscapeKey (event, result);

	return result;
}


GUIState& GUIView::BeginGUIState (InputEvent &event)
{
	GUIState& state = GetGUIState();
	m_KeyboardState.LoadIntoGUIState (state);
	state.SetEvent (event);
	state.BeginFrame ();
	return state;
}

void GUIView::EndGUIState (GUIState& state)
{
	m_KeyboardState.SaveFromGUIState (state);
	m_KeyboardState.EndFrame ();
}

void GUIView::Init (MonoBehaviour* behaviour, int depthBits, int antiAlias)
{
	g_GUIViews.insert(this);
	GetSceneTracker().AddSceneInspector(this);

	m_GameViewRect = Rectf (0, 0, 0, 0);
	m_GameViewPresentTime = 0.0f;

	m_AutoRepaint = false;
	m_NeedsRepaint = false;
	m_WantsMouseMove = false;
	m_View = [[GUIOpenGLView alloc] init];

	m_View->m_View = this;
	m_Instance = behaviour;

	m_DepthBits = depthBits;
	m_AntiAlias = antiAlias;
	DepthBufferFormat depthFormat = DepthBufferFormatFromBits(depthBits);
	[m_View setDepthFormat:depthFormat];
	[m_View setAntiAlias:antiAlias];
	[m_View setUseRealScreenCoords:YES];
}

GUIView::GUIView ()
{
	m_MouseRayInvisible = false;
	m_Window = nil;
}

GUIView::~GUIView ()
{
	// If we're the current executing GUIView, NULL out the lists in GUIManager
	GUIState &guiState = GetGUIState ();
	if (guiState.m_ObjectGUIState == &m_Instance->GetObjectGUIState())
	{
		EndGUIState (guiState);
		guiState.m_ObjectGUIState = NULL;
	}

	GetSceneTracker().RemoveSceneInspector(this);
	if( s_CurrentView == this )
		s_CurrentView = NULL;
	MonoViewData* viewData = GetMonoViewData();
	if (viewData)
		viewData->m_ViewPtr = NULL;

	g_GUIViews.erase (this);
	[m_View removeFromSuperview];
	[m_View release];
	m_View = nil;
}

void GUIView::RecreateContext( int depthBits, int antiAlias )
{
	m_DepthBits = depthBits;
	m_AntiAlias = antiAlias;
	DepthBufferFormat depthFormat = DepthBufferFormatFromBits(depthBits);
	[m_View setDepthFormat:depthFormat];
	[m_View setAntiAlias:antiAlias];
	[m_View RecreateContext];
}

void InvalidateGraphicsStateInEditorWindows()
{
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView& view = **i;
		GUIOpenGLView* glview = view.GetCocoaView();
		if( glview ) {
			[glview RecreateContext];
		}
	}
}

void RequestRepaintOnAllViews()
{
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView& view = **i;
		view.RequestRepaint();
	}
}

void ForceRepaintOnAllViews()
{
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView& view = **i;
		view.ForceRepaint();
	}
}

Vector2f GUIView::GetSize()
{
	NSRect r = [m_View frame];
	return Vector2f(r.size.width, r.size.height);
}

void GUIView::ForceRepaint()
{
	[m_View display];
}

void GUIView::RepaintAll (bool performAutorepaint)
{
	GUIView** needRepaint;
	ALLOC_TEMP(needRepaint, GUIView*, g_GUIViews.size());
	int repaintCount = 0;

	// Collect all windows that need a repaint and set the repaint flag
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView& view = **i;
		bool shouldAutorepaint = (view.m_AutoRepaint && performAutorepaint);
		if (view.m_NeedsRepaint || shouldAutorepaint)
			needRepaint[repaintCount++] = &view;
		view.m_NeedsRepaint = false;
	}

	// Actually repaint the collected windows
	for (int idx = 0; idx < repaintCount; idx++)
	{
		GUIView& view = *needRepaint[idx];
		// Make sure the window was not closed in the mean time
		if (g_GUIViews.count(&view) == 0)
			continue;

		if ([view.m_View lockFocusIfCanDraw])
		{
			const bool isGameView = view.IsGameView();
			[view.m_View setMeasureBlitTime: isGameView];
			[view.m_View drawRect: [view.m_View bounds]];
			[view.m_View unlockFocus];
			[view.m_View setNeedsDisplay: NO];
			if (isGameView)
			{
				view.m_GameViewPresentTime = [view.m_View getBlitTime];
				[view.m_View setMeasureBlitTime: false];
			}
			else
			{
				view.m_GameViewPresentTime = 0.0f;
			}
		}
	}

	GetApplication().PerformDrawHouseKeeping();
}

void GUIView::Repaint ()
{
	RequestRepaint ();
}

void GUIView::Focus ()
{
	[[m_View window]makeFirstResponder: m_View];
	[[m_View window]makeKeyAndOrderFront:nil];
	[NSApp activateIgnoringOtherApps:YES];
}


void GUIView::LostFocus ()
{
	if (GetApplication().MayUpdate())
		CallMethod ("OnLostFocus");

	GetAuxWindowManager ().OnLostFocus (this);
}


bool GUIView::GotFocus ()
{
	if (!GetApplication().MayUpdate())
		return false;

	if (m_Instance.IsValid() && m_Instance->GetInstance())
	{
		ScriptingMethodPtr method = m_Instance->FindMethod("OnFocus");
		if (method)
		{
			ScriptingInvocation invocation(method);
			invocation.object = m_Instance->GetInstance();

			if (MonoObjectToBool(invocation.InvokeChecked()))
			{
				// Close aux windows when focus changes.
				// EXCEPT: We don't want mouse events to close it (that gets handled later when we can cancel the event)
				//		Also, popup menus should not close aux windows (they are also used for tooltips)
				// TODO: Tooltips should NOT get focus!!!
				NSEventType t = [[NSApp currentEvent] type];
				if (t != NSLeftMouseDown &&
					t != NSRightMouseDown &&
					t != NSOtherMouseDown &&
				    m_Window->GetShowMode() != ContainerWindow::kShowPopupMenu &&
					m_Window->GetShowMode() != ContainerWindow::kShowAuxWindow)
				{
					GetAuxWindowManager ().OnGotFocus (this);
				}

				return true;
			}
		}
	}
	return false;
}

// Requests this GUIView to close itself
void GUIView::RequestClose ()
{
	[[GetCocoaView() window] performClose:nil];
}

void GUIView::AddToAuxWindowList ()
{
	GetAuxWindowManager ().AddView (this, GetCurrent ());
}

void GUIView::RemoveFromAuxWindowList ()
{
	GetAuxWindowManager ().RemoveView (this);
}

void GUIView::SetAsActiveWindow ()
{
	BeginCurrentContext();
}

void GUIView::BeginCurrentContext ()
{
	[m_View makeCurrentContext];
	GfxDevice& device = GetGfxDevice();
	device.SetInsideFrame(true);
}
void GUIView::EndCurrentContext ()
{
	GfxDevice& device = GetGfxDevice();
	device.SetInsideFrame(false);
}

void GUIView::SetWindow (ContainerWindow *win)
{
	m_Window = win;

	if (!win && [m_View window])
		[m_View removeFromSuperview];

	if (win && win->m_Window != [m_View window])
		[[win->m_Window contentView] addSubview: m_View];
}

void GUIView::SetPosition (const Rectf &position)
{
	NSView *content = [[m_View window] contentView];
	NSRect r = NSMakeRect (position.x, [content bounds].size.height - position.y - position.height, position.width, position.height);
	[m_View setFrame:r];
}

Rectf GUIView::GetPosition()
{
	NSRect r = [m_View frame];
	return Rectf(r.origin.x, r.origin.y, r.size.width, r.size.height);
}

void GUIView::CallMethod (const char* methodName)
{
	if (m_Instance.IsValid() && m_Instance->GetInstance())
		m_Instance->CallMethodInactive(methodName);
}

void GUIView::UpdateOSRects () {
	[[m_View window] invalidateCursorRectsForView:m_View];
}

void GUIView::StealMouseCapture()
{
	//We use this to make the window move waaay to the top (above) toolbar & dock
	[[m_View window] setLevel: kCGCursorWindowLevelKey];
}

void GUIView::UpdateScreenManager ()
{
	[GetCocoaView() UpdateScreenManager];
}

void GetGameViewAndRectAndFocus (Rectf* outGUIRect, NSView** outView, bool* hasFocus)
{
	GUIView* view = NULL;

	if (GUIView::GetStartView() && GUIView::GetStartView()->GetCocoaView())
	{
		view = GUIView::GetStartView();
	}
	else
	{
		// Go through all views and check if we have a game view
		for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
		{
			GUIView& curView = **i;
			if (curView.IsGameView() && curView.GetCocoaView())
			{
				view = &curView;
				break;
			}
		}
	}

	if (view && view->IsGameView())
	{
		GUIView* keyView = GetKeyGUIView();
		*hasFocus = (keyView == view);
		*outView = (NSView*)view->GetCocoaView();
		*outGUIRect = view->GetGameViewRect();
	}
	else
	{
		*hasFocus = false;
		*outView = NULL;
		*outGUIRect = Rectf(0,0,640,480);
	}
}


bool ExecuteCommandOnKeyWindow (const std::string& commandName)
{
	GUIView* view = GetKeyGUIView ();
	if (view)
	{
		view->UpdateScreenManager ();
		InputEvent validateEvent = InputEvent::CommandStringEvent (commandName, false);
		if (view->OnInputEvent(validateEvent))
		{
			InputEvent event = InputEvent::CommandStringEvent (commandName, true);
			return view->OnInputEvent(event);
		}
	}
	return false;
}

GUIView* GetMouseOverWindow (NSEvent* event)
{
	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView& guiview = **i;
		NSView* view = guiview.GetCocoaView();
		if ([event window] != [view window])
			continue;

		NSPoint p = [view convertPoint: [event locationInWindow] fromView: NULL];
		NSRect rect = [view bounds];
		if (NSPointInRect(p, rect))
			return &guiview;
	}
	return NULL;
}

void SetupEventValues (InputEvent *evt)
{
    evt->modifiers = ConvertCocoaModifierFlags ([[NSApp currentEvent] modifierFlags]);
}

// Find Mouse over view
GUIView* GetMouseOverWindow ()
{
	int windowCount;
	NSCountWindows(&windowCount);
	int windowList[windowCount];
	NSWindowList(windowCount, windowList);

	GUIView *found = NULL;
	int foundDepth = windowCount;

	for (GUIViews::iterator i=g_GUIViews.begin();i != g_GUIViews.end();i++)
	{
		GUIView& guiview = **i;
		NSView* view = guiview.GetCocoaView();
		// REFACTOR: Implement on windows side as well
		if (guiview.GetMouseRayInvisible())
			continue;
		NSWindow* win = [view window];
		NSPoint p = [view convertPoint: [win mouseLocationOutsideOfEventStream] fromView:nil];
		NSRect rect = [view bounds];
		int depth = windowCount;
		for(int winIndex = 0; winIndex < windowCount; winIndex++)
		{
			if(windowList[winIndex] == [win windowNumber])
			{
				depth = winIndex;
				break;
			}
		}

		if (NSPointInRect(p, rect))
		{
			if(depth <= foundDepth)
			{
				found = &guiview;
				foundDepth = depth;
			}
		}
	}
	return found;
}

Vector2f GetMousePosition()
{
	//Poll Mouse Window Position
	Point p;
	GetMouse (&p);

	return Vector2f(p.h, p.v);
}

void SendMouseMoveEvent (NSEvent* nsevent)
{
	GUIView* view = GetMouseOverWindow(nsevent);
	if (view == NULL || !view->GetWantsMouseMove())
		return;

	view->UpdateScreenManager ();
	InputEvent evt (nsevent, view->GetCocoaView());
	view->OnInputEvent (evt);
}

bool ExecuteCommandInMouseOverWindow (const std::string& commandName)
{
	// Find Mouse over view
	GUIView* mouseOverView = GetMouseOverWindow ();

	if (mouseOverView)
	{
		mouseOverView->UpdateScreenManager ();

		InputEvent validateEvent = InputEvent::CommandStringEvent (commandName, false);

		if (mouseOverView->OnInputEvent(validateEvent))
		{
			// Focus view
			[[mouseOverView->GetCocoaView() window]makeFirstResponder: mouseOverView->GetCocoaView()];

			// Send command
			// Validate event
			InputEvent event = InputEvent::CommandStringEvent (commandName, true, mouseOverView->GetCocoaView());
			return mouseOverView->OnInputEvent(event);
		}
	}
	return false;
}

void SetMainWindowDocumentEdited (bool edited)
{
	if (gMainWindow && gMainWindow->m_Window)
		[gMainWindow->m_Window setDocumentEdited: edited];
}

void SetMainWindowFileName (const std::string& title, const std::string& file)
{
	if (gMainWindow && gMainWindow->m_Window)
	{
		[gMainWindow->m_Window setTitle: MakeNSString(title)];
		[gMainWindow->m_Window setRepresentedFilename: MakeNSString (file)];
	}
}

Rectf ContainerWindow::FitWindowRectToScreen (const Rectf &rect, bool useMouseScreen, bool forceCompletelyVisible)
{
	NSArray* screens = [NSScreen screens];
	NSScreen* mainScreen = [screens objectAtIndex:0];
	NSRect screenFrame = [mainScreen frame];

	NSRect cocoaRect = NSMakeRect (rect.x, screenFrame.size.height - rect.y - rect.height, rect.width, rect.height);

	cocoaRect = [m_Window frameRectForContentRect: cocoaRect];

	NSPoint screenPos = [NSEvent mouseLocation];
	// Correct for bug in Apple's mouseLocation (returns y coordinate from 1-1280 instead of 0-1279)
	screenPos.y -= 1.0f;

	NSScreen *clampScreen = nil;
	if (useMouseScreen)
	{
		for(int i=0;i < [screens count];i++)
		{
			if(NSPointInRect(screenPos, [[screens objectAtIndex:i] frame]))
				clampScreen = [screens objectAtIndex:i];
		}
	}
	NSRect r = FitFrame (cocoaRect, clampScreen, forceCompletelyVisible ? 	kForceCompletelyVisible : kForceVisible);
	r = [m_Window contentRectForFrameRect: r];

	return Rectf (r.origin.x, NSHeight (screenFrame) - r.origin.y - r.size.height, r.size.width, r.size.height);
}
