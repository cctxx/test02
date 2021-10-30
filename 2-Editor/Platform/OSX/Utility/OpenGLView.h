#ifndef OPENGLVIEW_H
#define OPENGLVIEW_H

#import <Cocoa/Cocoa.h>
#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "PlatformDependent/OSX/UnicodeInputView.h"

#if SUPPORT_AGL
void SetContextDrawableFromView(GraphicsContextHandle context,NSView *view);
void UpdateContextDrawableFromView(GraphicsContextHandle context,NSView *view);
#endif

@interface OpenGLView : UnicodeInputView {
	GraphicsContextHandle m_Context;
	NSOpenGLContext *m_OpenGLContext;
	bool m_NeedsContextUpdate;
	NSRect m_LastFrame; // last frame in window coordinates
	IBOutlet id m_Delegate;
	BOOL m_RegisteredBoundsUpdate;
	/// Set this to true to make GUIToScreenCoordinates user real screen coordinate (like the editor)
	/// Games (and hence, gameView uses (0,0) as the topleft - no matter where they are on the physical screen - that's false :)
	bool m_UseRealScreenCoords;
	bool m_RecreateContext;
	bool m_MeasureBlitTime;
	float m_BlitTime;
	
	DepthBufferFormat m_DepthFormat;
	int m_AntiAlias;
	int m_ActualAntiAlias;
}
-(void)setUseRealScreenCoords:(BOOL)useThem;
-(void)setDepthFormat:(DepthBufferFormat)depth;
-(void)setAntiAlias:(int)antiAlias;
-(void)setMeasureBlitTime:(bool)measure;
-(float)getBlitTime;
- (void)setDelegate:(id)delegate;
- (void)UpdateScreenManager;
-(void)updateGLContext;
-(void)makeCurrentContext;
-(void) checkNeedsRecreateOnResize:(NSSize)size;
-(void)RecreateContext;
/// After all context state is setup renderRect is called!
/// [delegate renderRect: [NSView frame]]

@end

@protocol OpenGLViewDelegate 
	-(void)renderRect:(NSRect)rect;
@end

// Helper function: Get the absolute screen coordinate of the top-left corner of a view
// very useful for passing into GUIManager::SetEditorGUIInfo
NSPoint GetScreenPosForView (NSView *view);
#endif
