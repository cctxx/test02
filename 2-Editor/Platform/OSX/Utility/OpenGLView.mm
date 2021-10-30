#include "UnityPrefix.h"

#include "Runtime/GfxDevice/opengl/GLContext.h"
#include "OpenGLView.h"
#include "Editor/Platform/Interface/RepaintController.h"
#include "Editor/Src/Application.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/GfxDevice/VramLimits.h"

double GetTimeSinceStartup();

#if SUPPORT_AGL
void SetContextDrawableFromView(GraphicsContextHandle context,NSView *view)
{
	WindowRef win=(WindowRef)[[view window] windowRef];
	
	//@TODO: use [view visibleRect]; and setup an origin instead

	NSRect frame = [view frame];
	frame = [view convertRect: frame toView: NULL];
	SetContextDrawable(context,GetWindowPort(win), (const float*)&frame);
}

void UpdateContextDrawableFromView(GraphicsContextHandle context,NSView *view)
{
	//@TODO: use [view visibleRect]; and setup an origin instead
	NSRect frame = [view frame];
	frame = [view convertRect: frame toView: NULL];
	UpdateContextDrawable(context, (const float*)&frame);
}
#endif

@implementation OpenGLView

- (BOOL)sendInputEvent: (InputEvent&)ie {
	return NO;
}

- (void) awakeFromNib
{

}

- (id)initWithFrame:(NSRect) frameRect {	
	self = [super initWithFrame:frameRect]; 
	if(!self)
		NSLog(@"initWithFrame failed");

	m_NeedsContextUpdate = true;
	m_UseRealScreenCoords = true;
	m_MeasureBlitTime = false;
	m_BlitTime = 0.0f;
	m_DepthFormat = kDepthFormat24;
	m_AntiAlias = -1;
	m_ActualAntiAlias = 0;
	m_OpenGLContext = NULL;
 
	return self;
}

- (void) _surfaceNeedsUpdate:(NSNotification*)notification
{
	[self updateGLContext];
	[self setNeedsDisplay: true];
}

-(void)setMeasureBlitTime:(bool)measure
{
	m_MeasureBlitTime = measure;
}

-(float)getBlitTime
{
	return m_BlitTime;
}


-(void)setUseRealScreenCoords:(BOOL)useThem 
{
	m_UseRealScreenCoords = useThem;
}

-(void)setDepthFormat:(DepthBufferFormat)depth
{
	m_DepthFormat = depth;
}

-(void)setAntiAlias:(int)antiAlias
{
	m_AntiAlias = antiAlias;
}

-(void)RecreateContext
{
	m_RecreateContext = true;
//	LogString("Recreate context");
}

- (void)tryRegisterSetBoundsChangedNotification
{
	// Register a bounds changed observer with the scroll view.
	// This is ne
	if (!m_RegisteredBoundsUpdate)
	{
		NSView* viewHierarchy = [self superview];
		while (viewHierarchy != NULL)
		{
			if ([viewHierarchy isKindOfClass: [NSScrollView class]])
			{
				NSScrollView* scrollView = (NSScrollView*)viewHierarchy;
				NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
				[center addObserver: self
						selector: @selector(_surfaceNeedsUpdate:)
						name: NSViewBoundsDidChangeNotification
						object: [scrollView contentView]];
			
				[[scrollView contentView] setPostsBoundsChangedNotifications: YES];
				m_RegisteredBoundsUpdate = true;
				break;
			}
			
			viewHierarchy = [viewHierarchy superview];
		}
	
	}
}

NSPoint GetScreenPosForView (NSView *view) {
	NSPoint viewTopLeft = [view convertPoint: NSMakePoint (0, [view frame].size.height) toView: nil];
	NSPoint bottomLeft = [[view window] convertBaseToScreen: viewTopLeft];
//	NSRect screenRect = [[[view window] screen] frame];
//	bottomLeft.x -= screenRect.origin.x;
//	bottomLeft.y -= screenRect.origin.y;
	NSRect screenFrame = [[[NSScreen screens] objectAtIndex:0] frame];

	NSPoint topLeft = NSMakePoint (bottomLeft.x, screenFrame.size.height - bottomLeft.y);
	
	return topLeft;	
}

-(void) UpdateScreenManager
 {
	NSRect frame = [self frame];
	GetScreenManager().SetupScreenManagerEditor( frame.size.width, frame.size.height );
	GetRenderManager().SetWindowRect (Rectf (0,0, frame.size.width, frame.size.height));

	// Pass screen offsets to UnityGUI (for popping up popup menus at the correct place)
	NSPoint p;
	if (m_UseRealScreenCoords)
		p = GetScreenPosForView (self);
	else
		p = NSZeroPoint;
	Vector2f p2 (p.x, p.y);
	GetGUIManager().SetEditorGUIInfo (p2);
}

- (void)setDelegate:(id)delegate
{
	m_Delegate = delegate;
}

- (void) dealloc {
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	RemoveSceneRepaintView (self);

	if (m_OpenGLContext != NULL)
		[m_OpenGLContext release];

	ActivateMasterContextGL();
	DestroyContextGL (m_Context);
	
	[super dealloc];
}

-(BOOL) isOpaque {
	return YES;
}

 
- (BOOL)wantsDefaultClipping
{
	return NO;
}

- (void)viewDidMoveToWindow
{
	if( m_Context.IsValid() )
	{
		ActivateMasterContextGL();
		DestroyContextGL( m_Context );
		[self setNeedsDisplay: true];
	}
}

// Update the on-screen position & scrolling of the OpenGL context
-(void)updateGLContext {	
	NSRect totalFrame = [self convertRect: [self bounds] toView: NULL];

	bool didCreateContext = false;
	if( !m_Context.IsValid() )
	{
		int aa = m_AntiAlias;
		m_Context = MakeNewContext( totalFrame.size.width, totalFrame.size.height, false, true, false, m_DepthFormat, &aa, true );
		m_ActualAntiAlias = aa;
		
	#if SUPPORT_AGL
		SetContextDrawableFromView( m_Context, self );
	#else
		GraphicsContextGL &context = *OBJECT_FROM_HANDLE(m_Context, GraphicsContextGL);
		m_OpenGLContext = [[NSOpenGLContext alloc] initWithCGLContextObj: context.cgl];
		[m_OpenGLContext setView: self];	
	#endif
		didCreateContext = true;
	}
	
	// We also need to update drawable if the view has scrolled (this happens in
	// RenderTextureInspector as it can scroll). Hence we check whether current window rect
	// has changed from last known one.
	if( didCreateContext || m_NeedsContextUpdate )
	{
	#if SUPPORT_AGL
		UpdateContextDrawable(m_Context, (const float*)&totalFrame);
	#else
		[m_OpenGLContext update];
	#endif
	}
	
	// Forcing one delayed extra context update seems to fix an issue where the game view / scene view sometimes gets stuck.
	// There is most likely a deeper underlying issue, but for now this is all I could figure out to fix it.
	m_NeedsContextUpdate = didCreateContext;
	
	// We must set up a  bounds changed notification in order to be notified when the scroll view changes so we can sync the clip rects.
	[self tryRegisterSetBoundsChangedNotification];
	
	[self UpdateScreenManager];
}

- (void)drawRect:(NSRect)aRect
{
	if (!GetApplication().MayUpdate())
		return;

	bool contextWasRecreated = false;
	if (m_Context.IsValid() && m_RecreateContext)
	{
		// Prevent cocoa from drawing default cocoa background
		contextWasRecreated = true;
		NSDisableScreenUpdates();

		ActivateMasterContextGL();
		DestroyContextGL (m_Context);
		m_RecreateContext = false;
	}

	[self updateGLContext];
	
	if (!m_Context.IsValid())
	{
		ErrorString("Invalid OpenGL Context for rendering.");
		
		if (contextWasRecreated)
			NSDisableScreenUpdates();
			
		return;
	}
		
	SetMainGraphicsContext (m_Context);
	ActivateGraphicsContext (m_Context);
	
	GfxDevice& device = GetGfxDevice();
	device.SetInsideFrame(true);
	
	id target = m_Delegate;
	if (target == NULL)
		target = self;
	
	if ([target respondsToSelector: @selector (renderRect:)])
		[target renderRect: [self frame]];

	if (!m_Context.IsValid())
	{
		ErrorString("OpenGL Context became invalid during rendering");
		if (contextWasRecreated)
			NSDisableScreenUpdates();
		return;
	}
	
	double time0 = 0.0;
	if (m_MeasureBlitTime)
		time0 = GetTimeSinceStartup();
	PresentContextGL (m_Context);
	if (m_MeasureBlitTime)
		m_BlitTime = GetTimeSinceStartup() - time0;
	
	device.SetInsideFrame(false);
	
	// Prevent cocoa from drawing default cocoa background
	if (contextWasRecreated)
		NSEnableScreenUpdates();
}

-(void) makeCurrentContext {
	SetMainGraphicsContext (m_Context);
	ActivateGraphicsContext (m_Context);
}


// When resizing a view we normally do not recreate the context and just update it's view parameters.
// However, if changing the size would cause us to choose different AA level because of VRAM constraints,
// then we have to recreate the context.
-(void) checkNeedsRecreateOnResize:(NSSize)size
{
	int aa = m_AntiAlias;
	if (aa < 0)
	{
		if (GetQualitySettingsPtr())
			aa = GetQualitySettings().GetCurrent().antiAliasing;
		else
			aa = 0;
	}
	if (aa <= 1)
		return;
	
	int newAA = ChooseSuitableFSAALevel (size.width, size.height, 4, 4, 4, aa);
	if (newAA != m_ActualAntiAlias)
		m_RecreateContext = true;
}


- (void) setFrame:(NSRect)frame {
	[super setFrame: frame];
	m_NeedsContextUpdate = true;
	[self checkNeedsRecreateOnResize: frame.size];
}

- (void) setFrameOrigin:(NSPoint)origin {
	[super setFrameOrigin: origin];
	m_NeedsContextUpdate = true;
}

- (void) setFrameSize:(NSSize)size {
	[super setFrameSize: size];
	m_NeedsContextUpdate = true;
	[self checkNeedsRecreateOnResize: size];
}

@end