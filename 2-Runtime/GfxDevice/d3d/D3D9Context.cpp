#include "UnityPrefix.h"
#include "D3D9Context.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "D3D9Enumeration.h"
#include "D3D9Utils.h"
#include "GfxDeviceD3D9.h"
#include "TimerQueryD3D9.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Threads/ThreadSharedObject.h"
#include "Runtime/Misc/Plugins.h"
#if UNITY_EDITOR
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Camera/RenderManager.h"
#include "D3D9Window.h"
#endif

#if WEBPLUG
#define ENABLE_NV_PERFHUD 0
#else
#define ENABLE_NV_PERFHUD 1
#endif

#define ENABLE_D3D_WINDOW_LOGGING 1

static IDirect3D9*			s_D3D = NULL;
static IDirect3DDevice9*	s_Device = NULL;

static RenderColorSurfaceD3D9  s_BackBuffer;
static RenderDepthSurfaceD3D9  s_DepthStencil;
static HWND					s_Window = NULL;
static HINSTANCE			s_D3DDll = NULL;
static D3DPRESENT_PARAMETERS	s_PresentParams;
static D3D9FormatCaps*		s_FormatCaps = NULL;
static bool					s_CurrentlyWindowed = true;
static D3DDISPLAYMODE 		s_LastWindowedMode;
bool g_D3DUsesMixedVP = false;
bool g_D3DHasDepthStencil = true;
D3DFORMAT g_D3DDepthStencilFormat = D3DFMT_D16;
D3DDEVTYPE g_D3DDevType;
DWORD g_D3DAdapter = D3DADAPTER_DEFAULT;

#if WEBPLUG
extern bool gInsideFullscreenToggle;
#endif

typedef IDirect3D9* (WINAPI* Direct3DCreate9Func)(UINT);

GfxDeviceD3D9& GetD3D9GfxDevice();
void SetD3D9DeviceLost( bool lost ); // GfxDeviceD3D9.cpp
bool IsD3D9DeviceLost();
void ResetDynamicResourcesD3D9();

#if ENABLE_PROFILER
D3DPERF_BeginEventFunc g_D3D9BeginEventFunc;
D3DPERF_EndEventFunc g_D3D9EndEventFunc;
#endif


bool InitializeD3D(D3DDEVTYPE devtype)
{
	AssertIf( s_D3D || s_Device || s_Window || s_D3DDll || s_FormatCaps );
	g_D3DDevType = devtype;

	s_D3DDll = LoadLibrary( "d3d9.dll" );
	if( !s_D3DDll )
	{
		printf_console( "d3d: no D3D9 installed\n" );
		return false; // no d3d9 installed
	}

	Direct3DCreate9Func createFunc = (Direct3DCreate9Func)GetProcAddress( s_D3DDll, "Direct3DCreate9" );
	if( !createFunc )
	{
		printf_console( "d3d: Direct3DCreate9 not found\n" );
		FreeLibrary( s_D3DDll );
		s_D3DDll = NULL;
		return false; // for some reason Direct3DCreate9 not found
	}

	#if ENABLE_PROFILER
	g_D3D9BeginEventFunc = (D3DPERF_BeginEventFunc)GetProcAddress(s_D3DDll, "D3DPERF_BeginEvent");
	g_D3D9EndEventFunc = (D3DPERF_EndEventFunc)GetProcAddress(s_D3DDll, "D3DPERF_EndEvent");
	#endif

	// create D3D object
	s_D3D = createFunc( D3D_SDK_VERSION );
	if( !s_D3D )
	{
		printf_console( "d3d: no 9.0c available\n" );
		FreeLibrary( s_D3DDll );
		s_D3DDll = NULL;
		return false; // D3D initialization failed
	}

	// validate the adapter ordinal
	UINT adapterCount = s_D3D->GetAdapterCount();
	if ( g_D3DAdapter >= adapterCount )
		g_D3DAdapter = D3DADAPTER_DEFAULT;

	// check whether we have a HAL device
	D3DDISPLAYMODE mode;
	HRESULT hr;
	if (FAILED(hr = s_D3D->GetAdapterDisplayMode(g_D3DAdapter, &mode)))
	{
		printf_console ("d3d: failed to get adapter mode (adapter %d error 0x%08x)\n", g_D3DAdapter, hr);
		s_D3D->Release();
		s_D3D = NULL;
		FreeLibrary( s_D3DDll );
		s_D3DDll = NULL;
		return false; // failed to get adapter mode
	}
	if( FAILED( s_D3D->CheckDeviceType( g_D3DAdapter, g_D3DDevType, mode.Format, mode.Format, TRUE ) ) )
	{
		printf_console( "d3d: no support for this device type (accelerated/ref)\n" );
		s_D3D->Release();
		s_D3D = NULL;
		FreeLibrary( s_D3DDll );
		s_D3DDll = NULL;
		return false; // no HAL driver available
	}

	// enumerate all formats, multi sample types and whatnot
	s_FormatCaps = new D3D9FormatCaps();
	if( !s_FormatCaps->Enumerate( *s_D3D ) )
	{
		printf_console( "d3d: no video modes available\n" );
		return false;
	}

	return true;
}

IDirect3D9* GetD3DObject()
{
	AssertIf( !s_D3D );
	return s_D3D;
}
D3D9FormatCaps* GetD3DFormatCaps()
{
	AssertIf( !s_FormatCaps );
	return s_FormatCaps;
}

void CleanupD3D()
{
	AssertIf( s_Device || s_Window );

	delete s_FormatCaps;
	s_FormatCaps = NULL;

	if( s_D3D )
	{
		s_D3D->Release();
		s_D3D = NULL;
	}
	if( s_D3DDll )
	{
		FreeLibrary( s_D3DDll );
		s_D3DDll = NULL;
	}
}

D3DFORMAT GetD3DFormatForChecks()
{
	AssertIf( !s_FormatCaps );
	return s_FormatCaps->GetAdapterFormatForChecks();
}

static void SetFramebufferDepthFormat(GfxDevice* realDevice, D3DFORMAT format)
{
	// Not the most robust way to figure out the format, but should do.
	int depthBPP = GetBPPFromD3DFormat(format);
	DepthBufferFormat depthFormat = kDepthFormatNone;
	if (depthBPP == 16)
		depthFormat = kDepthFormat16;
	else if (depthBPP == 32)
		depthFormat = kDepthFormat24;
	realDevice->SetFramebufferDepthFormat(depthFormat);

	// Set it on the client device as well, if we're changing resolutions
	// and the property hasn't been propagated by copying from the real to client device.
	if (IsGfxDevice())
		GetGfxDevice().SetFramebufferDepthFormat(depthFormat);
}

bool InitializeOrResetD3DDevice(
	class GfxDevice* device,
	HWND window, int width, int height,
	int refreshRate, bool fullscreen, int vBlankCount, int fsaa,
	int& outBackbufferBPP, int& outFrontbufferBPP, int& outDepthBPP, int& outFSAA )
{
	AssertIf( !s_D3D );

	outBackbufferBPP = 4;
	outFrontbufferBPP = 4;
	outDepthBPP = 4;
	outFSAA = 0;

	width = std::max(width, 1);
	height = std::max(height, 1);

	D3DDISPLAYMODE mode;
	if( s_CurrentlyWindowed )
	{
		HRESULT hr = s_D3D->GetAdapterDisplayMode( g_D3DAdapter, &mode );
		if( FAILED( hr ) )
		{
			printf_console( "d3d initialize: failed to get adapter display mode [%s]\n", GetD3D9Error(hr) );
			return false;
		}
		s_LastWindowedMode = mode;
	}
	else
	{
		// If we are fullscreen right now, use last checked Windowed mode format
		// to choose compatible formats. Otherwise we won't be able to switch to 16 bit
		// desktop mode after a 32 bit fullscreen one.
		mode = s_LastWindowedMode;
	}

	D3DPRESENT_PARAMETERS& pparams = s_PresentParams;
	ZeroMemory (&pparams, sizeof(D3DPRESENT_PARAMETERS));
	pparams.BackBufferWidth = width;
	pparams.BackBufferHeight = height;
	pparams.BackBufferCount = 1;
	pparams.hDeviceWindow = window;
	pparams.FullScreen_RefreshRateInHz = fullscreen ? refreshRate : 0;

	pparams.EnableAutoDepthStencil = FALSE;
	g_D3DHasDepthStencil = true;

	pparams.Windowed = fullscreen ? FALSE : TRUE;
	pparams.SwapEffect = D3DSWAPEFFECT_DISCARD;

	// fullscreen FSAA might be buggy
	if( fullscreen && gGraphicsCaps.buggyFullscreenFSAA )
		fsaa = 1;

	s_FormatCaps->FindBestPresentationParams( width, height, mode.Format, !fullscreen, vBlankCount, fsaa, pparams );

	outBackbufferBPP = GetBPPFromD3DFormat(pparams.BackBufferFormat)/8;
	outFrontbufferBPP = GetBPPFromD3DFormat(mode.Format)/8;
	outDepthBPP = GetBPPFromD3DFormat(pparams.AutoDepthStencilFormat)/8;
	outFSAA = (pparams.MultiSampleType == D3DMULTISAMPLE_NONMASKABLE) ? pparams.MultiSampleQuality : pparams.MultiSampleType;
	g_D3DDepthStencilFormat = pparams.AutoDepthStencilFormat;
	device->SetCurrentTargetSize(pparams.BackBufferWidth, pparams.BackBufferHeight);
	SetFramebufferDepthFormat(device, pparams.AutoDepthStencilFormat);

	bool deviceInLostState = false;
	if( !s_Device )
	{
		AssertIf( s_Window );

		UINT adapterIndex = g_D3DAdapter;
		D3DDEVTYPE devType = g_D3DDevType;

		#if ENABLE_NV_PERFHUD
		UINT adapterCount = s_D3D->GetAdapterCount();
		D3DADAPTER_IDENTIFIER9 perfHudID;
		memset( &perfHudID, 0, sizeof(perfHudID) );
		s_D3D->GetAdapterIdentifier( adapterCount-1, 0, &perfHudID );
		perfHudID.Description[MAX_DEVICE_IDENTIFIER_STRING-1] = 0;
		if( strstr( perfHudID.Description, "PerfHUD" ) != NULL )
		{
			adapterIndex = adapterCount-1;
			devType = D3DDEVTYPE_REF;
		}
		#endif

		const int kShaderVersion11 = (1 << 8) + 1;
		bool hasHardwareTL = gGraphicsCaps.d3d.d3dcaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT;
		bool hasVS11 = LOWORD(gGraphicsCaps.d3d.d3dcaps.VertexShaderVersion) >= kShaderVersion11;
		DWORD behaviourFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		if( !hasVS11 )
			behaviourFlags = D3DCREATE_MIXED_VERTEXPROCESSING;
		if( !hasHardwareTL )
			behaviourFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
		g_D3DUsesMixedVP = (behaviourFlags == D3DCREATE_MIXED_VERTEXPROCESSING);

		if( GetGfxThreadingMode() == kGfxThreadingModeThreaded )
			behaviourFlags |= D3DCREATE_MULTITHREADED;

		// Preserve FPU mode. Benchmarking both in hardware and software vertex processing does not
		// reveal any real differences. If FPU mode is not preserved, bad things will happen, like:
		// * doubles will act like floats
		// * on Firefox/Safari, some JavaScript libraries will stop working (spect.aculo.us, dojo) - case 17513
		// * some random funky FPU exceptions will happen
		HRESULT hr = s_D3D->CreateDevice( adapterIndex, devType, window, behaviourFlags | D3DCREATE_FPU_PRESERVE, &pparams, &s_Device );
		if( FAILED( hr ) )
		{
			printf_console( "d3d: creation params: flags=%x swap=%i vsync=%x w=%i h=%i fmt=%i bbcount=%i dsformat=%i pflags=%x\n",
				behaviourFlags, pparams.SwapEffect, pparams.PresentationInterval,
				pparams.BackBufferWidth, pparams.BackBufferHeight, pparams.BackBufferFormat, pparams.BackBufferCount,
				pparams.AutoDepthStencilFormat, pparams.Flags );
			printf_console( "d3d: failed to create device [%s]\n", GetD3D9Error(hr) );
			if (devType == D3DDEVTYPE_REF)
			{
				winutils::AddErrorMessage("Reference Rasterizer was requested but is not available.\nPlease make sure you have DirectX SDK installed.");
				winutils::DisplayErrorMessagesAndQuit ("REFRAST not available");
			}
			return false;
		}
		s_CurrentlyWindowed = pparams.Windowed ? true : false;

		gGraphicsCaps.hasTimerQuery =
			(GetD3DDevice()->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, NULL) != D3DERR_NOTAVAILABLE) &&
			(GetD3DDevice()->CreateQuery(D3DQUERYTYPE_TIMESTAMP,NULL) != D3DERR_NOTAVAILABLE);
	}
	else
	{
		AssertIf( !s_Window );

		// If we're resetting device mid-frame (e.g. script calls Screen.SetResolution),
		// we need to end scene, reset and begin scene again.
		bool wasInsideFrame = GetD3D9GfxDevice().IsInsideFrame();
		if( wasInsideFrame )
		{
			s_Device->EndScene();
			GetD3D9GfxDevice().SetInsideFrame(false);
		}

		// cleanup
		s_BackBuffer.Release();
		s_DepthStencil.Release();

		PluginsSetGraphicsDevice (s_Device, kGfxRendererD3D9, kGfxDeviceEventBeforeReset);

		D3DPRESENT_PARAMETERS ppcopy = pparams; // copy them, as Reset changes some values
		HRESULT hr = s_Device->Reset( &ppcopy );
		if( FAILED(hr) )
		{
			if( hr == D3DERR_DEVICELOST )
			{
				deviceInLostState = true;
				SetD3D9DeviceLost( true );
			}
			else
			{
				ErrorString( Format("D3D device reset failed [%s]", GetD3D9Error(hr)) );
				return false;
			}
		}

		PluginsSetGraphicsDevice (s_Device, kGfxRendererD3D9, kGfxDeviceEventAfterReset);

		s_CurrentlyWindowed = ppcopy.Windowed ? true : false;
		if( wasInsideFrame && !deviceInLostState )
		{
			s_Device->BeginScene();
			GetD3D9GfxDevice().SetInsideFrame(true);
		}

#if ENABLE_PROFILER
		if (gGraphicsCaps.hasTimerQuery)
			GetD3D9GfxDevice().GetTimerQueries().RecreateAllQueries();
#endif
	}

	s_Window = window;
	if( !deviceInLostState )
	{
		s_Device->GetRenderTarget (0, &s_BackBuffer.m_Surface);
		s_BackBuffer.width = pparams.BackBufferWidth;
		s_BackBuffer.height = pparams.BackBufferHeight;
		// create depth stencil
		D3D9DepthStencilTexture depthStencil = CreateDepthStencilTextureD3D9 (s_Device, pparams.BackBufferWidth, pparams.BackBufferHeight, pparams.AutoDepthStencilFormat, pparams.MultiSampleType, pparams.MultiSampleQuality, TRUE);
		if (depthStencil.m_Surface)
		{
			s_DepthStencil.m_Surface = depthStencil.m_Surface;
			s_DepthStencil.m_Texture = depthStencil.m_Texture;
			s_DepthStencil.width = pparams.BackBufferWidth;
			s_DepthStencil.height = pparams.BackBufferHeight;
			s_DepthStencil.depthFormat = kDepthFormat16; //@TODO?
		}

		s_BackBuffer.backBuffer		= true;
		s_DepthStencil.backBuffer	= true;

	#if !UNITY_EDITOR
		RenderSurfaceHandle bbHandle(&s_BackBuffer), dsHandle(&s_DepthStencil);
		device->SetRenderTargets(1, &bbHandle, dsHandle);
	#endif
		s_Device->SetRenderState (D3DRS_ZENABLE, TRUE);
	}

	return true;
}

void GetBackBuffersAfterDeviceReset()
{
	AssertIf (!s_Device);
	AssertIf (!s_DepthStencil.m_Surface);
	s_BackBuffer.Release();
	s_Device->GetRenderTarget (0, &s_BackBuffer.m_Surface);
	s_BackBuffer.backBuffer = true;
}

#if UNITY_EDITOR
void EditorInitializeD3D(GfxDevice* device)
{
	int dummy;
	if( !InitializeOrResetD3DDevice( device, s_HiddenWindowD3D, 32, 32, 0, false, 0, 0, dummy, dummy, dummy, dummy ) )
	{
		winutils::AddErrorMessage( "Failed to create master Direct3D window" );
		DestroyGfxDevice();
		winutils::DisplayErrorMessagesAndQuit( "Failed to initialize 3D graphics" );
	}

	// Disable D3D Debug runtime in editor release mode:
	// VERTEXSTATS query is only available in Debug runtime.
	#if UNITY_RELEASE
	if (CheckD3D9DebugRuntime(GetD3DDevice()))
	{
		winutils::AddErrorMessage (
			"You are using Direct3D Debug Runtime, this is not supported by\r\n"
			"Unity. Switch to Retail runtime in DirectX Control Panel.");
		DestroyGfxDevice();
		winutils::DisplayErrorMessagesAndQuit ("D3D9 Debug Runtime is not supported");
	}
	#endif
}
#endif

bool FullResetD3DDevice()
{
	#if ENABLE_D3D_WINDOW_LOGGING
	printf_console("FullResetD3DDevice\n");
	#endif
	// destroy dynamic VBO / render textures and reset the device
	ResetDynamicResourcesD3D9();
	bool ok = ResetD3DDevice();
	if( ok )
		SetD3D9DeviceLost( false );
	return ok;
}

bool HandleD3DDeviceLost()
{
	#if ENABLE_D3D_WINDOW_LOGGING
	printf_console("HandleD3DDeviceLost\n");
	#endif
	HRESULT hr = s_Device->TestCooperativeLevel();
	bool ok = false;
	switch( hr )
	{
		// Is device actually lost?
		case D3D_OK:
		{
			ok = true;
			break;
		}
		// If device was lost, do not render until we get it back
		case D3DERR_DEVICELOST:
		{
			#if ENABLE_D3D_WINDOW_LOGGING
			printf_console("  HandleD3DDeviceLost: still lost\n");
			#endif
			break;
		}
		// If device needs to be reset, do that
		case D3DERR_DEVICENOTRESET:
		{
			#if ENABLE_D3D_WINDOW_LOGGING
			printf_console("  HandleD3DDeviceLost: needs reset, doing it\n");
			#endif
			ok = FullResetD3DDevice();
			break;
		}
	}

	if( !ok )
		return false;

	// device is not lost anymore, proceed
	#if ENABLE_D3D_WINDOW_LOGGING
	printf_console("D3Dwindow device not lost anymore\n");
	#endif
	GetBackBuffersAfterDeviceReset();
	SetD3D9DeviceLost( false );

	return true;
}

bool ResetD3DDevice()
{
	AssertIf( !s_D3D || !s_Device || !s_Window );

	#if ENABLE_D3D_WINDOW_LOGGING
	printf_console("ResetD3DDevice\n");
	#endif

	// cleanup
	s_BackBuffer.Release();
	s_DepthStencil.Release();

	#if ENABLE_D3D_WINDOW_LOGGING
	printf_console("dev->Reset\n");
	#endif

	D3DPRESENT_PARAMETERS ppcopy = s_PresentParams; // copy them, as Reset changes some values

	#if WEBPLUG
	// Reset sends WM_ACTIVATE message which makes Web Player exit fullscreen (unless gInsideFullscreenToggle is set).
	bool insideFullscreenToggle = gInsideFullscreenToggle;
	gInsideFullscreenToggle = true;
	#endif

	PluginsSetGraphicsDevice (s_Device, kGfxRendererD3D9, kGfxDeviceEventBeforeReset);

	HRESULT hr = s_Device->Reset( &ppcopy );

	#if WEBPLUG
	gInsideFullscreenToggle = insideFullscreenToggle;
	#endif

	bool setToLost = false;
	if( FAILED(hr) )
	{
		if( hr == D3DERR_DEVICELOST )
		{
			#if ENABLE_D3D_WINDOW_LOGGING
			printf_console("set device to lost\n");
			#endif
			SetD3D9DeviceLost( true );
			setToLost = true;
		}
		else
		{
			ErrorString( Format("D3D device reset failed [%s]", GetD3D9Error(hr)) );
			return false;
		}
	}
	else
	{
		PluginsSetGraphicsDevice (s_Device, kGfxRendererD3D9, kGfxDeviceEventAfterReset);

		s_Device->GetRenderTarget (0, &s_BackBuffer.m_Surface);
		s_BackBuffer.width = ppcopy.BackBufferWidth;
		s_BackBuffer.height = ppcopy.BackBufferHeight;
		// create depth stencil
		D3D9DepthStencilTexture depthStencil = CreateDepthStencilTextureD3D9 (s_Device, ppcopy.BackBufferWidth, ppcopy.BackBufferHeight, ppcopy.AutoDepthStencilFormat, ppcopy.MultiSampleType, ppcopy.MultiSampleQuality, TRUE);
		if (depthStencil.m_Surface)
		{
			s_DepthStencil.m_Surface = depthStencil.m_Surface;
			s_DepthStencil.m_Texture = depthStencil.m_Texture;
			s_DepthStencil.width = ppcopy.BackBufferWidth;
			s_DepthStencil.height = ppcopy.BackBufferHeight;
			s_DepthStencil.depthFormat = kDepthFormat16; //@TODO?
		}

		s_BackBuffer.backBuffer		= true;
		s_DepthStencil.backBuffer	= true;

	#if !UNITY_EDITOR
		RenderSurfaceHandle bbHandle(&s_BackBuffer), dsHandle(&s_DepthStencil);
		GetRealGfxDevice().SetRenderTargets(1, &bbHandle, dsHandle);
	#endif
		s_Device->SetRenderState (D3DRS_ZENABLE, TRUE);
	}
	s_CurrentlyWindowed = ppcopy.Windowed ? true : false;

	return !setToLost;
}

void DestroyD3DDevice()
{
	// This can happen when quiting from screen selector - window is not set up yet
	if( !s_Window || !s_Device )
		return;

	// cleanup
	s_BackBuffer.Release();
	s_DepthStencil.Release();
	s_Device->Release();
	s_Device = NULL;
	s_Window = NULL;
}

IDirect3DDevice9* GetD3DDevice()
{
	AssertIf( !s_Device );
	return s_Device;
}

IDirect3DDevice9* GetD3DDeviceNoAssert()
{
	return s_Device;
}



#if UNITY_EDITOR

#include "PlatformDependent/Win/WinUtils.h"

HWND s_HiddenWindowD3D = NULL;

bool CreateHiddenWindowD3D()
{
	AssertIf( s_HiddenWindowD3D );

	// Dummy master window is 64x64 in size. Seems that 32x32 is too small for Rage cards (produces internal driver errors in CreateDevice).
	s_HiddenWindowD3D = CreateWindowW(
		L"STATIC",
		L"UnityHiddenWindow",
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS |	WS_CLIPCHILDREN,
		0, 0, 64, 64,
		NULL, NULL,
		winutils::GetInstanceHandle(), NULL );
	if( !s_HiddenWindowD3D )
	{
		winutils::AddErrorMessage( "Failed to create hidden window: %s", WIN_LAST_ERROR_TEXT );
		return false;
	}

	return true;
}

void DestroyHiddenWindowD3D()
{
	AssertIf( !s_HiddenWindowD3D );
	DestroyWindow( s_HiddenWindowD3D );
	s_HiddenWindowD3D = NULL;
}

#endif
