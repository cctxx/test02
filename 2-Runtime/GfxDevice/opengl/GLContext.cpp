#include "UnityPrefix.h"
#include "GLContext.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#include <set>
#include "UnityGL.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Threads/Thread.h"
#if UNITY_WIN
#include "PlatformDependent/Win/WinUtils.h"
#endif
#if UNITY_OSX
#if SUPPORT_AGL
#include <AGL/agl.h>
#endif
#include <OpenGL/OpenGL.h>
#include "Runtime/Graphics/ScreenManager.h"
#elif UNITY_LINUX
#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#endif


// define to 1 to print lots of context info
#define DEBUG_GL_CONTEXT 0

#if UNITY_OSX
void CleanupMasterContextOSX (GraphicsContextGL* context); // GLContextOSX.cpp
#endif


static GraphicsContextHandle gMainGraphicsContext;
static GraphicsContextHandle gMasterGraphicsContext;
static GraphicsContexts gContexts;


GraphicsContexts& GetGLContexts()
{
	return gContexts;
}

GraphicsContextHandle GetMainGraphicsContext() 
{
	return gMainGraphicsContext;
}

void SetMainGraphicsContext( GraphicsContextHandle context )
{
	Assert(context.IsValid());
	gMainGraphicsContext = context;
}

GraphicsContextHandle GetMasterGraphicsContext()
{
	#if UNITY_WIN
	Assert(gMasterGraphicsContext.IsValid());
	#elif UNITY_OSX || UNITY_LINUX
	if( !gMasterGraphicsContext.IsValid() )
		MakeMasterGLContext();
	#else
	#error "Unknown platform"
	#endif
	
	return gMasterGraphicsContext;
}

bool IsMasterGraphicsContextValid()
{
	return gMasterGraphicsContext.IsValid();
}

void PresentContextGL( GraphicsContextHandle contextHandle )
{
	AutoGfxDeviceAcquireThreadOwnership autoOwn;
	if( !contextHandle.IsValid() )
		return;
	
	GraphicsContextGL* context = OBJECT_FROM_HANDLE(contextHandle,GraphicsContextGL);
	
#if UNITY_WIN
	BOOL ok = ::SwapBuffers( context->hdc );
	if( !ok ) {
		DWORD err = GetLastError();
	}
#elif UNITY_LINUX
	if (gGraphicsCaps.gl.hasArbSync)
	{
		if (context->presentSync)
		{
			OGL_CALL(glClientWaitSync(static_cast<GLsync> (context->presentSync), 0, GL_TIMEOUT_IGNORED));
			OGL_CALL(glDeleteSync(static_cast<GLsync> (context->presentSync)));
		}
		context->presentSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	}

	#if !WEBPLUG && !UNITY_EDITOR
	GLint curFBO;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &curFBO);
	GetScreenManager().PreBlit();
	#endif

	glXSwapBuffers(reinterpret_cast<Display*> (context->display), context->window);

	#if !WEBPLUG && !UNITY_EDITOR
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, curFBO);
	#endif
#elif UNITY_OSX
	if (gGraphicsCaps.gl.hasAppleFence)
	{
		if (!context->appleFenceValid)
		{
			OGL_CALL(glGenFencesAPPLE(1, &context->appleFence));
			context->appleFenceValid = true;
		}
		else
			OGL_CALL(glFinishFenceAPPLE(context->appleFence));

		OGL_CALL(glSetFenceAPPLE(context->appleFence));
	}

	
	#if WEBPLUG
	// No need to swap in this case.
	if ( GetScreenManager().IsUsingCoreAnimation() && !GetScreenManager().IsFullScreen() )
		return;
	#endif

	#if !WEBPLUG && !UNITY_EDITOR
	GLint curFBO;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &curFBO);
	GetScreenManager().PreBlit();
	#endif
	
	CGLFlushDrawable( context->cgl );
	#if !WEBPLUG && !UNITY_EDITOR
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, curFBO);
	#endif
#else
	#error "Unknown platform"
#endif
}

#if UNITY_OSX
void CleanupPresentSync (GraphicsContextGL* context)
{
	if (context == NULL)
		return;
	
	if (context->presentSync)
	{
		glDeleteSync(static_cast<GLsync> (context->presentSync));
		context->presentSync = NULL;
	}
	
	if (context->appleFenceValid)
	{
		glDeleteFencesAPPLE(1, &context->appleFence);
		context->appleFenceValid = false;
		context->appleFence = 0;
	}
}
#endif

#if UNITY_LINUX
void CleanupPresentSync (GraphicsContextGL* context)
{
	if (context && context->presentSync)
	{
		glDeleteSync(static_cast<GLsync> (context->presentSync));
		context->presentSync = NULL;
	}
}
#endif

void DestroyContextGL( GraphicsContextHandle& contextHandle ) 
{
	AutoGfxDeviceAcquireThreadOwnership autoOwn;
	ErrorIf( GetMasterGraphicsContext () == contextHandle );
	if ( !contextHandle.IsValid() )
		return;
	
	// prevent stale pointer to main context
	if( contextHandle == GetMainGraphicsContext() )
	{
		SetMainGraphicsContext( GetMasterGraphicsContext() );
	}

	GraphicsContextGL* context = OBJECT_FROM_HANDLE(contextHandle, GraphicsContextGL);
	
	gContexts.erase( *context );
	
	#if UNITY_WIN
	
	#if DEBUG_GL_CONTEXT
	printf_console( "CTX: destroy context %p\n", (DWORD)context->hglrc );
	#endif
	
	if (IsGfxDevice())
	{
		HGLRC curGL = wglGetCurrentContext ();
		HDC curDC = wglGetCurrentDC ();
		wglMakeCurrent (context->hdc, context->hglrc);
		GetRealGfxDevice().UnbindObjects();
		wglMakeCurrent (curDC, curGL);
	}
	
	wglDeleteContext( context->hglrc );
	ReleaseDC( context->hwnd, context->hdc );
	
	#elif UNITY_OSX
	
	CleanupPresentSync(context);
	
	#if DEBUG_GL_CONTEXT
	printf_console( "CTX: destroy context %p\n", context->cgl );
	#endif
	
	if (IsGfxDevice())
	{
		CGLContextObj currentCtx = CGLGetCurrentContext();
		CGLSetCurrentContext(context->cgl);
		GetRealGfxDevice().UnbindObjects();
		CGLSetCurrentContext (currentCtx);
	}
	
#if SUPPORT_AGL
	if (context->agl)
	{
		if( aglDestroyContext(context->agl)==GL_FALSE )
			printf_console( "aglDestroyContext failed!\n" );
	}
	else
#endif
	{
		CGLError err = CGLDestroyContext(context->cgl);
		if ( err )
			printf_console( "CGLDestroyContext failed: %s!\n", CGLErrorString(err) );
	}
	#elif UNITY_LINUX

	CleanupPresentSync(context);
	
	#if DEBUG_GL_CONTEXT
	printf_console( "CTX: destroy context %p\n", context->context);
	#endif
	
	Display *display = reinterpret_cast<Display*> (context->display);
	GLXContext glxcontext = reinterpret_cast <GLXContext> (context->context);
	if (IsGfxDevice())
	{
		GLXContext curGL = glXGetCurrentContext();
		GLXDrawable curDrawable = glXGetCurrentDrawable ();
		glXMakeCurrent(display, context->window, glxcontext);
		GetRealGfxDevice().UnbindObjects();
		glXMakeCurrent(display, curDrawable, curGL);
	}

	glXDestroyContext(display, glxcontext);

	#else
	#error "Unknown platform"
	#endif

	delete context;
	contextHandle.Reset();
}

void DestroyMainContextGL()
{
	AutoGfxDeviceAcquireThreadOwnership autoOwn;
	GraphicsContextHandle ctx = GetMainGraphicsContext();
	if( ctx == GetMasterGraphicsContext() )
		return;
	
	if( ctx.IsValid() )
	{
		glFinish();
		ActivateGraphicsContext( GetMasterGraphicsContext() );
		DestroyContextGL( ctx );
	}
	SetMainGraphicsContext( GetMasterGraphicsContext() );
}


void CleanupMasterContext()
{
	AutoGfxDeviceAcquireThreadOwnership autoOwn;
	Assert(gMasterGraphicsContext.IsValid());
	GraphicsContextGL* context = OBJECT_FROM_HANDLE(gMasterGraphicsContext,GraphicsContextGL);
	gContexts.erase( *context );
	
	#if UNITY_WIN

	#if DEBUG_GL_CONTEXT
	printf_console( "GLDebug context: cleanup master context %x %s\n", (DWORD)context->hglrc, GetMasterContextClassName().c_str() );
	#endif
	wglMakeCurrent(NULL, NULL);

	wglDeleteContext( context->hglrc );
	#if DEBUG_GL_CONTEXT
	printf_console( "GLDebug context: deleted glrc %x\n", context->hglrc );
	#endif
	ReleaseDC( context->hwnd, context->hdc );
	#if DEBUG_GL_CONTEXT
	printf_console( "GLDebug context: released dc %x for window %x\n", context->hdc, context->hwnd );
	#endif
	DestroyWindow( context->hwnd );
	#if DEBUG_GL_CONTEXT
	printf_console( "GLDebug context: destroyed window %x\n", context->hwnd );
	#endif
	winutils::UnregisterWindowClass( GetMasterContextClassName().c_str() );
	
	#elif UNITY_OSX
	
	CleanupMasterContextOSX (context);
	
	#elif UNITY_LINUX
	Display *display = reinterpret_cast <Display*> (context->display);
	glFlush ();
	glXMakeCurrent(display, None, NULL);
	glXDestroyContext(display, reinterpret_cast<GLXContext> (context->context));

	#else
	#error "Unknown platform"
	#endif

	delete context;
	gMasterGraphicsContext.Reset();

	#if !UNITY_EDITOR // editor does not do cleanup
	Assert(gContexts.empty());
	#endif
}


bool ActivateGraphicsContextGL( const GraphicsContextGL& context, int flags )
{
	Assert(IsRealGfxDeviceThreadOwner());
	bool result = true;
	
	// In Web Player, do not unbind GL objects here:
	// At least on Chrome/OSX, when switching to fullscreen (sometimes) calls
	// into the plugin with an already destroyed
	// GL context set as active and so we crash if we use that.
	// Instead, unbind happens elsewhere for that case.
	//
	// However, have to unbind in the editor. Mostly when switching
	// graphics emulation, multiple contexts for each view can get old
	// stale state and picking would get wrong, case 408464.
	#if !WEBPLUG
	if( !(flags & kGLContextSkipUnbindObjects) && IsGfxDevice() )
		GetRealGfxDevice().UnbindObjects();
	#endif
	
	#if UNITY_WIN
	Assert(context.hglrc);

	if( !wglMakeCurrent(context.hdc, context.hglrc) ) 
	{
		printf_console( "GLContext: failed to activate %x: %s\n", context.hglrc, WIN_LAST_ERROR_TEXT );
		result = false;
	}
	
	#elif UNITY_OSX
	
	Assert(context.cgl != NULL);
	#if DEBUG_GL_CONTEXT
	char const* master = gMasterGraphicsContext.object ? (&context == gMasterGraphicsContext.object ? " (MASTER)" : "") : " (MASTER NULL)";
	printf_console( "CTX: activate %p%s\n", context.cgl, master );
	#endif
	// In general the previous context needs to be flushed whenever switching to a new one.
	// This is even more important in case of multithreaded GL, where not flushing can
	// result in random crashes.
	//
	// However, don't flush if explicitly told not to. We don't flush when activating context
	// at start of web player loop, just after creating web player window, and so on (i.e. whenever
	// context activation happens because we initialize something). Skipping flush gets rid of
	// "first frame shows garbage or previous game" in most cases.
	if( !(flags & kGLContextSkipFlush) && CGLGetCurrentContext() != NULL)
		glFlush();
	
	CGLError err = CGLSetCurrentContext(context.cgl);
	if( err )
		printf_console( "GLContext: CGLSetCurrentContext() failed: %s\n", CGLErrorString(err) );

	#elif UNITY_LINUX
	Assert (context.window);

	if( !(flags & kGLContextSkipFlush) && glXGetCurrentContext() != NULL)
		glFlush();

	glXMakeCurrent(reinterpret_cast<Display*> (context.display), context.window, reinterpret_cast<GLXContext> (context.context));

	#else
	#error "Unknown platform"
	#endif
	
	if( !(flags & kGLContextSkipUnbindObjects) && IsGfxDevice() )
	{
		// Must unbind objects otherwise stuff falls apart in other places, like read pixels stop
		// to work when OpenGL ES 1.1 is being emulated in the editor.
		GetRealGfxDevice().UnbindObjects();
	}
	if( !(flags & kGLContextSkipInvalidateState) && IsGfxDevice() )
	{
		// In some cases we cannot invalidate, e.g. when destroying render textures (case 490767).
		GetRealGfxDevice().InvalidateState();
	}
	return result;
}


bool ActivateGraphicsContext (GraphicsContextHandle ctx, bool currentThreadOnly, int flags)
{
	bool isMainThread = Thread::CurrentThreadIsMainThread();
	if (isMainThread && IsGfxDevice())
	{
		GetGfxDevice().AcquireThreadOwnership();
	}
#if ENABLE_PROFILER
	bool activeTimerQueries = false;
	if (IsGfxDevice())
	{
		activeTimerQueries = GetRealGfxDevice().TimerQueriesIsActive();
		if (activeTimerQueries)
			 GetRealGfxDevice().EndTimerQueries();
	}
#endif
	Assert(isMainThread || IsRealGfxDeviceThreadOwner());
	Assert (gMasterGraphicsContext.IsValid());
	GraphicsContextGL* context = OBJECT_FROM_HANDLE(ctx,GraphicsContextGL);
	bool res = ActivateGraphicsContextGL( *context, flags );
#if ENABLE_PROFILER
	if (activeTimerQueries)
		GetRealGfxDevice().BeginTimerQueries();
#endif
	if (isMainThread && IsGfxDevice())
	{
		GetGfxDevice().ReleaseThreadOwnership();
	}
	if (!currentThreadOnly && IsGfxDevice() && isMainThread)
	{
		GetGfxDevice().SetActiveContext (context);
	}
	return res;
}

bool ActivateMasterContextGL()
{
	GraphicsContextHandle ctx = GetMasterGraphicsContext();
	ErrorIf( !ctx.IsValid() );
	return ActivateGraphicsContext (ctx);
}

bool ActivateMainContextGL ()
{
	GraphicsContextHandle ctx = GetMainGraphicsContext();
	if( ctx.IsValid() )return ActivateGraphicsContext (ctx);
	return false;
}

void AssignMasterGraphicsContextGL( GraphicsContextGL* ctx )
{
	Assert(!gMasterGraphicsContext.IsValid());
	gMasterGraphicsContext.object = ctx;
}



#if UNITY_OSX

GraphicsContextGL GetCurrentGraphicsContext ()
{
	GraphicsContextGL ctx;
	ctx.cgl = CGLGetCurrentContext();
#if SUPPORT_AGL
	ctx.agl = aglGetCurrentContext();
#endif
	return ctx;
}

#elif UNITY_WIN

GraphicsContextGL GetCurrentGraphicsContext ()
{
	GraphicsContextGL ctx;
	ctx.hdc = wglGetCurrentDC();
	ctx.hglrc = wglGetCurrentContext();
	return ctx;
}

#elif UNITY_LINUX

GraphicsContextGL GetCurrentGraphicsContext ()
{
	GraphicsContextGL ctx;
	ctx.context = glXGetCurrentContext();
	ctx.window = (Window)glXGetCurrentDrawable();
	ctx.display = GetDisplay();
	return ctx;
}

#else
#error "Unknown platform"
#endif
