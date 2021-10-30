#include "UnityPrefix.h"

#if !UNITY_WP8 && !UNITY_METRO

#include "D3D11Context.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "D3D11Includes.h"
#include "D3D11Utils.h"
#include "TexturesD3D11.h"
#include "TimerQueryD3D11.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Utilities/LogUtility.h"
#include "PlatformDependent/Win/ComPtr.h"
#include "Runtime/Utilities/Argv.h"


SupportedFeatureLevels GetSupportedFeatureLevels()
{
	SupportedFeatureLevels features;

	if (HasARGV("force-feature-level-9-1")) features.push_back(D3D_FEATURE_LEVEL_9_1);
	if (HasARGV("force-feature-level-9-2")) features.push_back(D3D_FEATURE_LEVEL_9_2);
	if (HasARGV("force-feature-level-9-3")) features.push_back(D3D_FEATURE_LEVEL_9_3);
	if (HasARGV("force-feature-level-10-0")) features.push_back(D3D_FEATURE_LEVEL_10_0);
	if (HasARGV("force-feature-level-10-1")) features.push_back(D3D_FEATURE_LEVEL_10_1);
	if (HasARGV("force-feature-level-11-0")) features.push_back(D3D_FEATURE_LEVEL_11_0);

	features.push_back(D3D_FEATURE_LEVEL_11_0);
	features.push_back(D3D_FEATURE_LEVEL_10_1);
	features.push_back(D3D_FEATURE_LEVEL_10_0);

	return features;
}

#if ENABLE_PROFILER
D3D11PERF_BeginEventFunc g_D3D11BeginEventFunc = NULL;
D3D11PERF_EndEventFunc g_D3D11EndEventFunc = NULL;
#endif

using namespace win;

#if !UNITY_RELEASE
#define UNITY_DX11_CREATE_FLAGS D3D11_CREATE_DEVICE_DEBUG
#else
#define UNITY_DX11_CREATE_FLAGS 0
#endif


static D3D_FEATURE_LEVEL kSupportedFeatureLevels[] = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
};


bool InitD3D11RenderDepthSurface (RenderDepthSurfaceD3D11& rs, TexturesD3D11* textures, bool sampleOnly);


static ComPtr<ID3D11Device> s_Device = NULL;
// DX11.1 runtime only. On older runtimes this will stay null. Internally accessed by GetGfxDevice11_1().
static ComPtr<ID3D11Device1> s_Device11_1;

static ID3D11DeviceContext* s_Context = NULL;
static IDXGIFactory* s_DXGIFactory = NULL;
IDXGIFactory* GetDXGIFactory() { return s_DXGIFactory; }
static IDXGISwapChain* s_SwapChain = NULL;
static IDXGIOutput* s_Output = NULL;
static int s_SwapChainAA = -1;
IDXGISwapChain* GetD3D11SwapChain() { return s_SwapChain; }

static int s_SyncInterval = 0;
int GetD3D11SyncInterval() { return s_SyncInterval; }

static RenderColorSurfaceD3D11  s_BackBuffer;
static RenderDepthSurfaceD3D11  s_DepthStencil;

ID3D11RenderTargetView* g_D3D11CurrRT;
ID3D11DepthStencilView* g_D3D11CurrDS;
ID3D11Resource* g_D3D11CurrRTResource;
ID3D11Resource* g_D3D11CurrDSResource;
RenderColorSurfaceD3D11* g_D3D11CurrColorRT;
RenderDepthSurfaceD3D11* g_D3D11CurrDepthRT;
int g_D3D11Adapter = 0;
int g_D3D11Output = 0;

static HWND					s_Window = NULL;
static HINSTANCE			s_D3DDll = NULL;
static HINSTANCE			s_D3D9Dll = NULL;
static HINSTANCE			s_DXGIDll = NULL;
static bool					s_CurrentlyWindowed = true;

typedef HRESULT (WINAPI* D3D11CreateDeviceFunc)(
	IDXGIAdapter *pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	CONST D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	ID3D11Device **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel,
	ID3D11DeviceContext **ppImmediateContext
);

typedef HRESULT (WINAPI* CreateDXGIFactoryFunc)(
	REFIID ridd,
	void** ppFactory
);

// Either selects default adapter (NULL) or n-th one,
// defined by adapterIndex. Returned adapter
// must be released if not NULL!
static IDXGIAdapter* SelectAdapter (int adapterIndex)
{
	s_DXGIDll = LoadLibrary( "dxgi.dll" );
	if( !s_DXGIDll )
		return NULL;

	CreateDXGIFactoryFunc createDXGIFactory = (CreateDXGIFactoryFunc)GetProcAddress( s_DXGIDll, "CreateDXGIFactory" );
	if( !createDXGIFactory )
		return NULL;

	IDXGIAdapter* adapter = NULL;
	if ( SUCCEEDED(createDXGIFactory(__uuidof(IDXGIFactory), (void**)&s_DXGIFactory)) )
	{
		for ( int i = 0; SUCCEEDED(s_DXGIFactory->EnumAdapters(i, &adapter)); ++i )
		{
			if ( i == adapterIndex )
				break;
			else
				adapter->Release();
		}
	}

	return adapter;
}

// Selects default output (NULL) or the one defined by outputIndex.
// Result must be released!
static IDXGIOutput* SelectOutput (IDXGIAdapter* adapter, int outputIndex)
{
	if (outputIndex == 0)
		return NULL;

	Assert(adapter);

	IDXGIOutput* output = NULL;
	for ( int i = 0; SUCCEEDED(adapter->EnumOutputs(i, &output)); ++i )
	{
		if ( i == outputIndex )
			break;
		else
			output->Release();
	}

	return output;
}

bool InitializeD3D11 ()
{
	AssertIf (s_Device || s_Context || s_Window || s_D3DDll || s_D3D9Dll || s_DXGIDll);

	SupportedFeatureLevels features = GetSupportedFeatureLevels();
	IDXGIAdapter* adapter = NULL;

	s_D3DDll = LoadLibrary( "d3d11.dll" );
	if (!s_D3DDll)
	{
		printf_console ("d3d11: no D3D11 installed\n");
		goto _cleanup;
	}

	D3D11CreateDeviceFunc createFunc = (D3D11CreateDeviceFunc)GetProcAddress( s_D3DDll, "D3D11CreateDevice" );
	if( !createFunc )
	{
		printf_console ("d3d11: D3D11CreateDevice not found\n");
		goto _cleanup;
	}


	DWORD d3d11CreateFlags = 0;
	if (!HasARGV("force-d3d11-no-singlethreaded"))
	{
		d3d11CreateFlags |= D3D11_CREATE_DEVICE_SINGLETHREADED;
	}

	adapter = SelectAdapter (g_D3D11Adapter);
	if (adapter)
		s_Output = SelectOutput (adapter, g_D3D11Output);

	// create D3D device & immediate context,
	// with debug layer in Debug config
	HRESULT hr = E_FAIL;
	D3D_FEATURE_LEVEL level;
	D3D_DRIVER_TYPE driverType = adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
	#if !UNITY_RELEASE 
	hr = createFunc (
		adapter, driverType, NULL,
		d3d11CreateFlags | D3D11_CREATE_DEVICE_DEBUG,
		&features[0],
		features.size(),
		D3D11_SDK_VERSION, &s_Device, &level, &s_Context
		);
	#endif
	// create without debug layer if the above failed or was not called at all
	if (FAILED(hr))
	{
		hr = createFunc (
			adapter, driverType, NULL,
			d3d11CreateFlags,
			&features[0], features.size(),
			D3D11_SDK_VERSION, &s_Device, &level, &s_Context
			);
	}
	if (FAILED(hr))
	{
		printf_console( "d3d11: failed to create D3D11 device (0x%08x)\n", hr );
		goto _cleanup;
	}

	SAFE_RELEASE(adapter);

	// Query DX11.1 interface, may well fail for older runtimes and is silently ignored.
	s_Device->QueryInterface(&s_Device11_1);

	// Create DXGIFactory if it isn't already created by SelectAdapter
	if (!s_DXGIFactory)
	{
		IDXGIDevice* dxgiDevice = NULL;
		hr = s_Device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
		IDXGIAdapter* dxgiAdapter = NULL;
		hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);
		hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&s_DXGIFactory);
		dxgiAdapter->Release();
		dxgiDevice->Release();
	}


	#if ENABLE_PROFILER
	// Even on D3D11, PIX event marker functions are in D3D9 DLL
	s_D3D9Dll = LoadLibrary ("d3d9.dll");
	if (s_D3D9Dll)
	{
		g_D3D11BeginEventFunc = (D3D11PERF_BeginEventFunc)GetProcAddress(s_D3D9Dll, "D3DPERF_BeginEvent");
		g_D3D11EndEventFunc = (D3D11PERF_EndEventFunc)GetProcAddress(s_D3D9Dll, "D3DPERF_EndEvent");
	}
	#endif

	return true;

_cleanup:
	SAFE_RELEASE(adapter);
	SAFE_RELEASE(s_Output);
	SAFE_RELEASE(s_DXGIFactory);

	s_Device11_1.Free();
	s_Device.Free();

	SAFE_RELEASE(s_Context);

	if (s_D3DDll) {
		FreeLibrary (s_D3DDll);
		s_D3DDll = NULL;
	}
	if (s_D3D9Dll) {
		FreeLibrary (s_D3D9Dll);
		s_D3D9Dll = NULL;
	}
	if (s_DXGIDll) {
		FreeLibrary (s_DXGIDll);
		s_DXGIDll = NULL;
	}
	return false;
}


void CleanupD3D11()
{
	AssertIf (((ID3D11Device *)s_Device) || s_Context || s_Window);

	if (s_D3DDll)
	{
		FreeLibrary (s_D3DDll);
		s_D3DDll = NULL;
	}
	if (s_D3D9Dll)
	{
		FreeLibrary (s_D3D9Dll);
		s_D3D9Dll = NULL;
	}
	if (s_DXGIDll) {
		FreeLibrary (s_DXGIDll);
		s_DXGIDll = NULL;
	}
}

static void ReleaseBackbufferResources()
{
	//NESTED_LOG("DX11 debug", "ReleaseBackbufferResources");

	Assert(s_Device && s_Context);

	#if ENABLE_PROFILER
	g_TimerQueriesD3D11.ReleaseAllQueries();
	#endif

	SAFE_RELEASE(s_DepthStencil.m_Texture);
	SAFE_RELEASE(s_DepthStencil.m_SRView);
	SAFE_RELEASE(s_DepthStencil.m_DSView);
	s_BackBuffer.Reset();

	if (s_Context)
		s_Context->OMSetRenderTargets(0, NULL, NULL);
}


static void CreateBackbufferResources(GfxDevice* device, int width, int height, int antiAlias, DXGI_FORMAT swapFormat, bool sRGB)
{
	//NESTED_LOG("DX11 debug", "CreateBackbufferResources %ix%i", width, height);

	HRESULT hr;

	// Set the Backbuffer primary format flags
	s_BackBuffer.flags = sRGB ? (s_BackBuffer.flags | kSurfaceCreateSRGB) : (s_BackBuffer.flags & ~kSurfaceCreateSRGB);

	// Backbuffer
	s_BackBuffer.width = width;
	s_BackBuffer.height = height;
	s_BackBuffer.samples = antiAlias;
	s_BackBuffer.backBuffer = true;
	s_BackBuffer.format = kRTFormatARGB32;

	hr = s_SwapChain->GetBuffer (0, __uuidof(*s_BackBuffer.m_Texture), (void**)&s_BackBuffer.m_Texture);
	Assert(SUCCEEDED(hr));
	SetDebugNameD3D11 (s_BackBuffer.m_Texture, Format("SwapChain-BackBuffer-Texture-%dx%d", width, height));

	// Create the primary backbuffer view
	ID3D11RenderTargetView* rtv = NULL;
	D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
	rtDesc.Format = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
	rtDesc.ViewDimension = antiAlias > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
	rtDesc.Texture2D.MipSlice = 0;
	hr = s_Device->CreateRenderTargetView (s_BackBuffer.m_Texture, &rtDesc, &rtv);
	s_BackBuffer.SetRTV (0, 0, false, rtv);
	Assert(SUCCEEDED(hr));
	SetDebugNameD3D11 (rtv, Format("SwapChain-BackBuffer-RTV-%dx%d", width, height));

	// Create the secondary backbuffer view
	D3D11_RENDER_TARGET_VIEW_DESC rtDescSecondary;
	rtDescSecondary.Format = !sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
	rtDescSecondary.ViewDimension = antiAlias > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
	rtDescSecondary.Texture2D.MipSlice = 0;
	hr = s_Device->CreateRenderTargetView (s_BackBuffer.m_Texture, &rtDescSecondary, &rtv);
	s_BackBuffer.SetRTV (0, 0, true, rtv);
	Assert(SUCCEEDED(hr));
	SetDebugNameD3D11 (rtv, Format("SwapChain-BackBuffer-RTVSec-%dx%d", width, height));

	if (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0)
	{
		// Create shader resource view
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = swapFormat;
		srvDesc.ViewDimension = antiAlias > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		ID3D11ShaderResourceView* srView = NULL;
		hr = s_Device->CreateShaderResourceView (s_BackBuffer.m_Texture, &srvDesc, &s_BackBuffer.m_SRView);
		Assert (SUCCEEDED(hr));
		SetDebugNameD3D11 (s_BackBuffer.m_SRView, Format("SwapChain-BackBuffer-SRV-%dx%d", width, height));
	}

	// Depth stencil
	s_DepthStencil.width = width;
	s_DepthStencil.height = height;
	s_DepthStencil.samples = antiAlias;
	s_DepthStencil.dim = kTexDim2D;
	s_DepthStencil.backBuffer = true;
	s_DepthStencil.depthFormat = device->GetFramebufferDepthFormat();
	{
		bool dsOk = InitD3D11RenderDepthSurface (s_DepthStencil, NULL, false);
		Assert (dsOk);
	}

#if !UNITY_EDITOR
	RenderSurfaceHandle bbHandle(&s_BackBuffer), dsHandle(&s_DepthStencil);
	device->SetRenderTargets(1, &bbHandle, dsHandle);
#endif

	#if ENABLE_PROFILER
	if (gGraphicsCaps.hasTimerQuery)
		g_TimerQueriesD3D11.RecreateAllQueries();
	#endif
}


bool InitializeOrResetD3D11SwapChain(
	class GfxDevice* device,
	HWND window, int width, int height,
	int refreshRate, bool fullscreen, int vsynccount, int antiAlias,
	int& outBackbufferBPP, int& outFrontbufferBPP, int& outDepthBPP, int& outFSAA )
{
	Assert(s_Device && s_Context);

	ReleaseBackbufferResources();

	outBackbufferBPP = 4;
	outFrontbufferBPP = 4;
	outDepthBPP = 4;
	outFSAA = 0;

	device->SetCurrentWindowSize (width, height);

	// pick supported AA level
	if (antiAlias == -1)
		antiAlias = GetQualitySettings().GetCurrent().antiAliasing;
	while (antiAlias > 1 && !(gGraphicsCaps.d3d11.msaa & (1<<antiAlias)))
		--antiAlias;
	antiAlias = std::max(antiAlias, 1);

	const bool sRGB = GetActiveColorSpace() == kLinearColorSpace;

	// Release old swap chain if we need to change AA
	if (s_SwapChain && s_SwapChainAA != antiAlias)
	{
		// swap chain must go out of fullscreen before releasing it
		s_SwapChain->SetFullscreenState (FALSE, NULL);
		SAFE_RELEASE(s_SwapChain);
	}

	// Create Swap Chain
	HRESULT hr;

	DWORD swapFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	DXGI_FORMAT swapFormat = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
	s_SwapChainAA = antiAlias;
	s_SyncInterval = vsynccount;
	if (!s_SwapChain)
	{
		//NESTED_LOG("DX11 debug", "InitializeOrResetD3D11SwapChain, init %ix%i, window %p", width, height, window);

		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory (&sd, sizeof(sd));
		sd.BufferCount = 1;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = swapFormat;
		sd.BufferDesc.RefreshRate.Numerator = refreshRate;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		if (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0)
			sd.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
		sd.Flags = swapFlags;
		sd.OutputWindow = window;
		sd.SampleDesc.Count = antiAlias;
		sd.SampleDesc.Quality = 0;
		// Docs suggest always setting this to true and then doing SetFullscreenState
		sd.Windowed = TRUE;
		hr = s_DXGIFactory->CreateSwapChain (s_Device, &sd, &s_SwapChain);
		Assert(SUCCEEDED(hr));

		// We'll handle Alt-Enter and other things ourselves
		DWORD dxgiFlags = DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES;
		s_DXGIFactory->MakeWindowAssociation (window, dxgiFlags);

		if (fullscreen)
			s_SwapChain->SetFullscreenState(TRUE, s_Output);

		CreateBackbufferResources(device, width, height, antiAlias, swapFormat, sRGB);
	}
	else
	{
		DXGI_MODE_DESC mode;
		mode.Width = width;
		mode.Height = height;
		mode.RefreshRate.Numerator = refreshRate;
		mode.RefreshRate.Denominator = 1;
		mode.Format = swapFormat;
		mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		//NESTED_LOG("DX11 debug", "InitializeOrResetD3D11SwapChain, resize %ix%i, fs=%i, window %p", width, height, fullscreen, window);

		// Note: the following will often call WM_SIZE on us, which will handle actual resizing (since size might be slightly different!),
		// which will go into ResizeSwapD3D11SwapChain.

		{
			//NESTED_LOG("DX11 debug", "ResizeTarget 1st");
			s_SwapChain->ResizeTarget (&mode);
		}
		{
			//NESTED_LOG("DX11 debug", "SetFullscreenState");
			s_SwapChain->SetFullscreenState (fullscreen, fullscreen ? s_Output : NULL);
		}

		// according to "DXGI: Best Practices" on MSDN, advisable
		// to call resize target again with refresh rate zeroed out.
		mode.RefreshRate.Numerator = 0;
		mode.RefreshRate.Denominator = 0;
		if (fullscreen)
		{
			//NESTED_LOG("DX11 debug", "ResizeTarget 2nd");
			s_SwapChain->ResizeTarget (&mode);
		}

		// In some cases above calls do not post WM_SIZE, so ResizeSwapD3D11SwapChain is not called,
		// which means we won't get back buffer.
		// If we still don't have back buffer here, just set it up.
		if (!s_BackBuffer.m_Texture)
		{
			CreateBackbufferResources (device, width, height, s_SwapChainAA, swapFormat, sRGB);
		}
	}

	return true;
}


void ResizeSwapD3D11SwapChain (class GfxDevice* device, HWND window, int width, int height)
{
	if (!s_SwapChain)
		return;

	const bool hadBackBuffer = (s_DepthStencil.m_Texture != NULL);
	if (hadBackBuffer)
		ReleaseBackbufferResources();

	device->SetCurrentWindowSize (width, height);

	const bool sRGB = GetActiveColorSpace() == kLinearColorSpace;
	DWORD swapFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	DXGI_FORMAT swapFormat = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

	//NESTED_LOG("DX11 debug", "ResizeSwapD3D11SwapChain, resize %ix%i", width, height);
	s_SwapChain->ResizeBuffers (1, width, height, sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM, swapFlags);

	CreateBackbufferResources (device, width, height, s_SwapChainAA, swapFormat, sRGB);
}



void DestroyD3D11Device()
{
	// This can happen when quiting from screen selector - window is not set up yet
	if( !((ID3D11Device *)s_Device)|| !s_Context || !s_DXGIFactory )
		return;

	// swap chain must go out of fullscreen before releasing it
	if (s_SwapChain)
	{
		s_SwapChain->SetFullscreenState (FALSE, NULL);
	}

	// cleanup
	SAFE_RELEASE(s_DepthStencil.m_Texture);
	SAFE_RELEASE(s_DepthStencil.m_SRView);
	SAFE_RELEASE(s_DepthStencil.m_DSView);
	s_BackBuffer.Reset();
	SAFE_RELEASE(s_SwapChain);

	s_Context->ClearState();
	s_Context->Flush();

	s_Context->Release();
	s_Context = NULL;

	/*
	// Helper code to report any live objects
	ID3D11Debug *d3dDebug = NULL;
	if (SUCCEEDED(s_Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug)))
	{
		d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_SUMMARY);
		d3dDebug->Release();
	}
	*/

	s_Device11_1.Free();
	s_Device.Free();

	SAFE_RELEASE(s_Output);
	SAFE_RELEASE(s_DXGIFactory);
	
	s_Window = NULL;
}

ID3D11Device* GetD3D11Device()
{
	AssertIf( !((ID3D11Device *)s_Device) );
	return s_Device;
}

ID3D11Device1* GetD3D11_1Device()
{
	return s_Device11_1;
}


ID3D11DeviceContext* GetD3D11Context(bool expectNull)
{
	if (!expectNull)
		Assert( s_Context );
	return s_Context;
}

#endif

