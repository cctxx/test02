#pragma once

#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

#if UNITY_WIN
	struct GraphicsContextGL
	{
		HDC   hdc;
		HGLRC hglrc;
		HWND  hwnd;

		GraphicsContextGL() : hdc(NULL), hwnd(NULL), hglrc(NULL) 
		{ 
		}
		~GraphicsContextGL() 
		{ 
			hdc = NULL;hwnd = NULL; hglrc = NULL;
		}

		bool IsValid()const { return hglrc != NULL; }
		friend bool operator == (const GraphicsContextGL& lhs, const GraphicsContextGL& rhs) { return lhs.hglrc == rhs.hglrc; }
		friend bool operator < (const GraphicsContextGL& lhs, const GraphicsContextGL& rhs) { return lhs.hglrc < rhs.hglrc; }
		friend bool operator != (const GraphicsContextGL& lhs, const GraphicsContextGL& rhs) { return lhs.hglrc != rhs.hglrc; }
	};
#elif UNITY_OSX
	#define SUPPORT_AGL (!UNITY_64)
	struct __AGLContextRec;
	typedef struct __AGLContextRec       *AGLContext;
	
	struct _CGLContextObject;
	typedef struct _CGLContextObject     *CGLContext;

	struct _CGLPixelFormatObject;
	typedef struct _CGLPixelFormatObject   *CGLPixelFormatObj;

	// 10.5 SDK defines all CGL functions to use GLint, whereas 10.4 is long. Make this work in both.
	#ifndef MAC_OS_X_VERSION_MAX_ALLOWED
		#error Fail
	#elif MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5
		#define CGLint long
	#else
		#define CGLint int
	#endif
	
	struct GraphicsContextGL
	{
		CGLContext cgl;
	#if SUPPORT_AGL
		AGLContext agl;
	#endif
		
		void*      presentSync;
		uint       appleFence;
		bool       appleFenceValid;
		
		GraphicsContextGL() {
			cgl = NULL;
		#if SUPPORT_AGL
			agl = NULL;
		#endif
			presentSync = 0;
			appleFence = 0;
			appleFenceValid = false;
		}

		friend bool operator == (const GraphicsContextGL& lhs, const GraphicsContextGL& rhs) { return lhs.cgl == rhs.cgl; }
		friend bool operator < (const GraphicsContextGL& lhs, const GraphicsContextGL& rhs) { return lhs.cgl < rhs.cgl; }
		friend bool operator != (const GraphicsContextGL& lhs, const GraphicsContextGL& rhs) { return lhs.cgl != rhs.cgl; }
	};
	
	#define CGL_FROM_HANDLE(context) (*OBJECT_FROM_HANDLE(context,GraphicsContextGL)).cgl
#if SUPPORT_AGL
	#define AGL_FROM_HANDLE(context) (*OBJECT_FROM_HANDLE(context,GraphicsContextGL)).agl
#endif
#elif UNITY_LINUX
	#include "UnityGL.h"
	#include "PlatformDependent/Linux/X11Quarantine.h"

	struct GraphicsContextGL
	{
		NativeDisplayPtr display;
		NativeWindow window;
		void *context;
		void *presentSync;

		GraphicsContextGL() : window(0), context(NULL), presentSync(NULL)
		{
		}
		~GraphicsContextGL()
		{
			window = 0;
			context = NULL;
		}

		bool IsValid()const { return window != 0; }
		friend bool operator == (const GraphicsContextGL& lhs, const GraphicsContextGL& rhs) { return lhs.context == rhs.context; }
		friend bool operator < (const GraphicsContextGL& lhs, const GraphicsContextGL& rhs) { return lhs.context < rhs.context; }
		friend bool operator != (const GraphicsContextGL& lhs, const GraphicsContextGL& rhs) { return lhs.context != rhs.context; }
	};
	#define GLX_FROM_HANDLE(context) (*OBJECT_FROM_HANDLE(context,GraphicsContextGL)).context
#else
	#error "Unknown platform"
#endif

// Returns the master context
// Mac: if necessary creates it
// Windows: explicitly create master context earlier, with CreateMasterGraphicsContext()
GraphicsContextHandle GetMasterGraphicsContext();
bool IsMasterGraphicsContextValid();

void SetMainGraphicsContext( GraphicsContextHandle ctx );
GraphicsContextHandle GetMainGraphicsContext ();

void DestroyContextGL( GraphicsContextHandle& context );
void DestroyMainContextGL();

// Makes the context active. And sets up some default state.
bool ActivateGraphicsContext (GraphicsContextHandle ctx, bool currentThreadOnly = false, int flags = 0);
bool ActivateMasterContextGL ();
bool ActivateMainContextGL ();
bool ActivateGraphicsContextGL								( const GraphicsContextGL& ctx, int flags );
void AssignMasterGraphicsContextGL							( GraphicsContextGL* ctx );
GraphicsContextGL GetCurrentGraphicsContext                 ();

void PresentContextGL										( GraphicsContextHandle ctx );

#if UNITY_WIN
	void SetMasterContextClassName							( const std::wstring& windowClassName );
	const std::wstring& GetMasterContextClassName			();

	GraphicsContextHandle SetupGraphicsContextFromWindow	( HWND window, int width, int height, int inFSAA, int& outFSAA );
#endif




typedef std::set<GraphicsContextGL> GraphicsContexts;
GraphicsContexts& GetGLContexts();

enum {
	kGLContextSkipInvalidateState = 1 << 0,
	kGLContextSkipUnbindObjects = 1 << 1,
	kGLContextSkipFlush = 2 << 2,
};


#if UNITY_OSX
	void CleanupMasterContext();

	GraphicsContextHandle MakeNewContext( int width, int height, int fullscreen, bool doubleBuffer, bool pbuffer, DepthBufferFormat depthFormat, int* inoutAA, bool agl);
	GraphicsContextHandle MakeOffScreenContext (int width, int height, int depthBits, int stencilBits);
	GraphicsContextGL MakeNewContextGL( int width, int height, int fullscreen, bool doubleBuffer, bool pbuffer, bool depthTexture, DepthBufferFormat depthFormat, int* inoutAA, bool agl);
	GraphicsContextGL MakeOffScreenContextGL (int width, int height, int depthBits, int stencilBits);
	void GeneratePixelAttributes (int bits, int depthBits, int stencilBits, const void* cglPixelFormatAttributes, bool doubleBuffer,bool fullscreen, int supersample, bool pbuffer);
	void SetupDefaultContextState( GraphicsContextGL &context, bool setMultiThreaded, int fsaa );

	void CleanupPresentSync (GraphicsContextGL* context);

	void SetSyncToVBL (GraphicsContextHandle context, int syncCount);

	void SetDisplayID( CGDirectDisplayID display );

	void MakeMasterGLContext();

	#if UNITY_EDITOR
	void GLFinishAllGraphicsContexts ();
	void SetContextDrawable( GraphicsContextHandle context, CGrafPtr port, const float* frame );
	void UpdateContextDrawable( GraphicsContextHandle context, const float* frame );
	#endif
#elif UNITY_LINUX
	void CleanupMasterContext();
	void CleanupPresentSync (GraphicsContextGL* context);

	GraphicsContextHandle MakeNewContext( int width, int height, int fullscreen, bool doubleBuffer, bool pbuffer, DepthBufferFormat depthFormat, int antiAlias);
	GraphicsContextHandle MakeOffScreenContext (int width, int height, int depthBits, int stencilBits);
	GraphicsContextGL  MakeNewContextGL (NativeDisplayPtr display, NativeWindow window, int width, int height, int fullscreen, bool doubleBuffer, bool pbuffer, bool depthTexture, DepthBufferFormat depthFormat, int antiAlias);
	GraphicsContextGL MakeOffScreenContextGL (int width, int height, int depthBits, int stencilBits);
	void SetupDefaultContextState( GraphicsContextGL &context, bool setMultiThreaded, int fsaa );

	void SetSyncToVBL (GraphicsContextHandle context, int syncCount);
	void SetDisplay(NativeDisplayPtr display);
	NativeDisplayPtr GetDisplay();

	void MakeMasterGLContext();
	void MakeMasterGLContext(NativeDisplayPtr display, NativeWindow window);
	GraphicsContextGL* MakeGLContextForWindow(NativeDisplayPtr display, NativeWindow window, bool shareWithMaster=true);
	VisualInfoPtr CreateVisualInfo (NativeDisplayPtr display, int antialiasingLevel=0);

	#if UNITY_EDITOR
	void GLFinishAllGraphicsContexts ();
	void UpdateContextDrawable( GraphicsContextHandle context, const float* frame );
	GraphicsContextHandle SetupGraphicsContextFromWindow (NativeWindow window, int width, int height, int inFSAA, int& outFSAA);
	#endif
#endif
