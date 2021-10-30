#include "UnityPrefix.h"
#include "ContextGLES30.h"
#include "IncludesGLES30.h"
#include "AssertGLES30.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Misc/QualitySettings.h"

#if GFX_SUPPORTS_OPENGLES30

#if UNITY_WIN

#define EGL_OPENGL_ES3_BIT_KHR 0x00000040


struct EGLESData
{
	void*	dsp;
	void*	cfg;
	void*	cxt;
	void*	surf;
	EGLESData():dsp(NULL),cfg(NULL),cxt(NULL),surf(NULL){}
};


static EGLESData sOpenGLESData;

bool InitializeGLES30	()
{
	HWND hwnd = GetScreenManager().GetWindow();
	if (hwnd)
	{
		CreateContextGLES30 (hwnd);
		return true;
	}
	ErrorString ("gles20: Can't initialize because HWND not set up");
	return false;
}
void ShutdownGLES30	()
{
	DestroyContextGLES30();
}
bool IsContextGLES30Created()
{
	return sOpenGLESData.surf != NULL &&
		sOpenGLESData.cxt  != NULL &&
		sOpenGLESData.cfg  != NULL &&
		sOpenGLESData.dsp != NULL;
}

bool CreateContextGLES30(HWND hWnd)
{
	//Just in case
	DestroyContextGLES30();

	EGLint numConfigs;
	EGLint majorVersion;
	EGLint minorVersion;

#if UNITY_WIN

	int sampleCount = GetQualitySettings().GetCurrent().antiAliasing;

	/// Build up the attribute list
	const EGLint configAttribs[] =
	{
		EGL_LEVEL,				0,
		EGL_SURFACE_TYPE,		EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE,	EGL_OPENGL_ES3_BIT_KHR,
		EGL_NATIVE_RENDERABLE,	EGL_FALSE,
		EGL_DEPTH_SIZE,			16,
		EGL_ALPHA_SIZE,			1,
		EGL_STENCIL_SIZE,		1,
		EGL_SAMPLES,			sampleCount,
		EGL_NONE
	};

	// Get Display
	sOpenGLESData.dsp = eglGetDisplay( hWnd?GetDC(hWnd):EGL_DEFAULT_DISPLAY );
	if ( sOpenGLESData.dsp == EGL_NO_DISPLAY )
	{
	  printf_console("GLES30: eglGetDisplay failed\n" );
	  return false;
	}
	//Hack : eglInitialize invokes WM_ACTIVATE message, and gAppActive is already true, so Unity will try to call some functions which requires some initialization,
	//       and this is not done yet
	extern bool gAlreadyClosing;
	bool last = gAlreadyClosing;
	gAlreadyClosing = true;
	// Initialize EGL
	if ( ! eglInitialize( sOpenGLESData.dsp, &majorVersion, &minorVersion) )
	{
		printf_console("GLES30: eglInitialize failed\n");
		return false;
	}


	// Choose config
	if ( !eglChooseConfig(sOpenGLESData.dsp, configAttribs, &sOpenGLESData.cfg, 1, &numConfigs) )
	{
		printf_console("GLES30: eglChooseConfig failed\n");
		return false;
	}


	// Create a surface
	sOpenGLESData.surf = eglCreateWindowSurface( sOpenGLESData.dsp, sOpenGLESData.cfg, NativeWindowType( hWnd ), NULL );
	if ( sOpenGLESData.surf == EGL_NO_SURFACE )
	{
		printf_console("GLES30: eglCreateWindowSurface failed\n");
		return false;
	}

	// Create a GL context
	EGLint ctxAttribList[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
	sOpenGLESData.cxt = eglCreateContext( sOpenGLESData.dsp, sOpenGLESData.cfg, EGL_NO_CONTEXT, ctxAttribList );
	if ( sOpenGLESData.cxt == EGL_NO_CONTEXT )
	{
		printf_console("GLES30: eglCreateContext failed\n");
		return false;
	}   

	// Make the context current
	if ( ! eglMakeCurrent( sOpenGLESData.dsp, sOpenGLESData.surf, sOpenGLESData.surf, sOpenGLESData.cxt ) )
	{
	  printf_console("GLES30: eglMakeCurrent failed\n");
	  return false;
	}

	gAlreadyClosing = last;
#endif

	GLESAssert();	

	return true;
}


void DestroyContextGLES30()
{
	if(sOpenGLESData.dsp)
	{
		eglMakeCurrent(sOpenGLESData.dsp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ;
		eglDestroyContext( sOpenGLESData.dsp, sOpenGLESData.cxt );
		eglDestroySurface( sOpenGLESData.dsp, sOpenGLESData.surf );	
		eglTerminate( sOpenGLESData.dsp);
	}
	sOpenGLESData.surf = NULL;
	sOpenGLESData.cxt  = NULL;
	sOpenGLESData.cfg  = NULL;
	sOpenGLESData.dsp = NULL;
}
void PresentContextGLES30()
{
	eglSwapBuffers( sOpenGLESData.dsp, sOpenGLESData.surf );
}

void AcquireGLES30Context()
{
	if(sOpenGLESData.dsp)
	{
		// Make the context current
		if ( ! eglMakeCurrent( sOpenGLESData.dsp, sOpenGLESData.surf, sOpenGLESData.surf, sOpenGLESData.cxt ) )
		{
			printf_console("GLES30: eglMakeCurrent failed\n");
		}
	}
}

void ReleaseGLES30Context()
{
	if(sOpenGLESData.dsp)
	{
		eglMakeCurrent(sOpenGLESData.dsp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ;
	}
}


#elif UNITY_LINUX

static EGLDisplay gEGLDisplay = EGL_NO_DISPLAY;
static EGLConfig gEGLConfig;
static EGLSurface gEGLSurface = EGL_NO_SURFACE;
static EGLContext gEGLContext = EGL_NO_CONTEXT;

void SetEGLDisplay(EGLDisplay display)
{
	gEGLDisplay = display;
}

void SetEGLConfig(const EGLConfig &config)
{
	gEGLConfig = config;
}

bool InitializeGLES30	()
{
	Window window = 0;
	window = GetScreenManager().GetWindow();

	if(window)
	{
		CreateContextGLES30(window);
		return true;
	}

	return false;
}

void ShutdownGLES30		()
{
	DestroyContextGLES30();
}

bool IsContextGLES30Created()
{
	return gEGLSurface != EGL_NO_SURFACE && gEGLContext != EGL_NO_CONTEXT;
}

bool CreateContextGLES30(Window window)
{
	DestroyContextGLES30();

	gEGLSurface = eglCreateWindowSurface(gEGLDisplay, gEGLConfig, window, NULL);
	if(gEGLSurface == EGL_NO_SURFACE)
	{
		printf_console("eglCreateWindowSurface failed\n");
		return false;
	}

	// Create a context
	printf_console("Creating context\n");
	EGLint ctxAttribList[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
	gEGLContext = eglCreateContext(gEGLDisplay, gEGLConfig, EGL_NO_CONTEXT, ctxAttribList );
	if ( gEGLContext == EGL_NO_CONTEXT )
	{
		printf_console( "eglCreateContext failed\n" );
		return false;
	}

	if(!eglMakeCurrent(gEGLDisplay, gEGLSurface, gEGLSurface, gEGLContext))
	{
		printf_console("eglMakeCurrent failed\n");
	}

	return true;
}

void DestroyContextGLES30()
{
	if(IsContextGLES30Created())
	{
		eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(gEGLDisplay, gEGLContext);
		eglDestroySurface(gEGLDisplay, gEGLSurface);
	}

	gEGLSurface = EGL_NO_SURFACE;
	gEGLContext = EGL_NO_CONTEXT;
}

void PresentContextGLES()
{
	eglSwapBuffers(gEGLDisplay, gEGLSurface);
}

void AcquireGLES30Context()
{
	if(gEGLDisplay)
	{
		// Make the context current
		if(!eglMakeCurrent(gEGLDisplay, gEGLSurface, gEGLSurface, gEGLContext))
		{
			printf_console("GLES30: eglMakeCurrent failed\n");
		}
	}
}

void ReleaseGLES30Context()
{
	if(gEGLDisplay)
	{
		eglMakeCurrent(gEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	}
}


#endif // UNITY_WIN
#endif // GFX_SUPPORTS_OPENGLES30
