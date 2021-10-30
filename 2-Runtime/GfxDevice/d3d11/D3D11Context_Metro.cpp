#include "UnityPrefix.h"

#if UNITY_METRO
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
#include "PlatformDependent/MetroPlayer/AppCallbacks.h"
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
	features.push_back(D3D_FEATURE_LEVEL_9_3);
	features.push_back(D3D_FEATURE_LEVEL_9_2);
	features.push_back(D3D_FEATURE_LEVEL_9_1);

	return features;
}

#if ENABLE_PROFILER
D3D11PERF_BeginEventFunc g_D3D11BeginEventFunc = NULL;
D3D11PERF_EndEventFunc g_D3D11EndEventFunc = NULL;
#endif

using namespace win;

#if !UNITY_RELEASE && !defined(__arm__)
#define UNITY_DX11_CREATE_FLAGS D3D11_CREATE_DEVICE_DEBUG
#else
#define UNITY_DX11_CREATE_FLAGS 0
#endif


static ComPtr<ID3D11Device1> s_Device;
static ComPtr<ID3D11DeviceContext1> s_Context;
static ComPtr<IDXGIFactory2> s_DXGIFactory;
static IDXGISwapChain1* s_SwapChain = NULL;
#if UNITY_METRO_VS2013
static IDXGIDevice3* s_DXGIDevice3 = NULL;
#endif
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

static const int kSwapChainBackBufferCount = 2;
#if ENABLE_DX11_FRAME_LATENCY_WAITABLE_OBJECT
static bool s_EnableLowLatencyPresentationAPI = true;
static HANDLE s_FrameLatencyWaitableObject = NULL;
static UINT kSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
#else
static UINT kSwapChainFlags = 0;
#endif
static const DXGI_FORMAT kSwapChainBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

#define DX_CHECK(HR, ...) if (FAILED(hr)) {ErrorStringMsg(__VA_ARGS__); goto error; }

bool InitializeD3D11()
{
	HRESULT hr;

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<IDXGIDevice1> dxgiDevice;
	ComPtr<IDXGIAdapter> dxgiAdapter;

	#if UNITY_DX11_CREATE_FLAGS
	ComPtr<ID3D11Debug> debug;
	#endif

	Assert(!s_Device);
	Assert(!s_Context);
	Assert(!s_DXGIFactory);

	SupportedFeatureLevels features = GetSupportedFeatureLevels();

	D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;

	if (HasARGV("force-driver-type-warp"))
	{
		driverType = D3D_DRIVER_TYPE_WARP;
	}

	DWORD d3d11CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	if (!HasARGV("force-d3d11-no-singlethreaded"))
	{
		d3d11CreateFlags |= D3D11_CREATE_DEVICE_SINGLETHREADED;
	}
#if ENABLE_DX11_FRAME_LATENCY_WAITABLE_OBJECT
	if (HasARGV("disable-low-latency-presentation-api"))
	{
		printf_console("Disabling Low Latency presentation API.\n");
		s_EnableLowLatencyPresentationAPI = false;
		kSwapChainFlags = 0;
	}
#endif

	D3D_FEATURE_LEVEL level;
	hr = D3D11CreateDevice(
		nullptr,
		driverType,
		nullptr,
		(d3d11CreateFlags | UNITY_DX11_CREATE_FLAGS),
		&features[0],
		features.size(),
		D3D11_SDK_VERSION,
		&device,
		&level,
		&context);

    if (FAILED(hr) && driverType != D3D_DRIVER_TYPE_WARP)
    {
		WarningStringMsg("D3D11 failed to create with D3D_DRIVER_TYPE_HARDWARE, fallbacking D3D_DRIVER_TYPE_WARP...");
		driverType = D3D_DRIVER_TYPE_WARP;
        hr = D3D11CreateDevice(
			nullptr,
			driverType,
			nullptr,
			(d3d11CreateFlags | UNITY_DX11_CREATE_FLAGS),
			&features[0],
			features.size(),
			D3D11_SDK_VERSION,
			&device,
			&level,
			&context);
    }

	DX_CHECK(hr, "D3D11CreateDevice failed with error 0x%08x ", hr);

	hr = device->QueryInterface(&s_Device);
	DX_CHECK(hr, "device->QueryInterface(&s_Device) failed with error 0x%08x ", hr);

	hr = context->QueryInterface(&s_Context);
	DX_CHECK(hr, "context->QueryInterface(&s_Context) failed with error 0x%08x ", hr);

	#if UNITY_DX11_CREATE_FLAGS
	hr = s_Device->QueryInterface(&debug);
	AssertMsg(SUCCEEDED(hr), "s_Device->QueryInterface(&debug) failed failed with error 0x%08x", hr);

	if (SUCCEEDED(hr))
	{
		hr = debug->SetFeatureMask(D3D11_DEBUG_FEATURE_FLUSH_PER_RENDER_OP);
		AssertMsg(SUCCEEDED(hr), "debug->SetFeatureMask(D3D11_DEBUG_FEATURE_FLUSH_PER_RENDER_OP) failed with error 0x%08x", hr);
	}

	#endif
	#if UNITY_METRO_VS2013
	hr = s_Device->QueryInterface(__uuidof(IDXGIDevice3), (void**) &s_DXGIDevice3);
	DX_CHECK(hr, "s_Device->QueryInterface(__uuidof(IDXGIDevice3), (void**) &s_DXGIDevice3) with error 0x%08x ", hr);
	#endif

	hr = s_Device->QueryInterface(&dxgiDevice);
	DX_CHECK(hr, "s_Device->QueryInterface(&dxgiDevice) failed with error 0x%08x ", hr);

#if ENABLE_DX11_FRAME_LATENCY_WAITABLE_OBJECT
	// The maximum frame latency should be 1, if game is running at 60 FPS, but because
	// most games will probably be running around 30, I guess it's better to set 2
	hr = dxgiDevice->SetMaximumFrameLatency(s_EnableLowLatencyPresentationAPI ? 2 : 1);
#else
	hr = dxgiDevice->SetMaximumFrameLatency(1);
#endif
	DX_CHECK(hr, "dxgiDevice->SetMaximumFrameLatency(1)) failed with error 0x%08x ", hr);

	hr = dxgiDevice->GetAdapter(&dxgiAdapter);
	DX_CHECK(hr, "dxgiDevice->GetAdapter(&dxgiAdapter) failed with error 0x%08x ", hr);

	hr = dxgiAdapter->GetParent(__uuidof(s_DXGIFactory), reinterpret_cast<void**>(&s_DXGIFactory));	// ???
	DX_CHECK(hr, "dxgiAdapter->GetParent(...) failed with error 0x%08x ", hr);

	return true;



error:

	s_DXGIFactory.Free();
	s_Context.Free();
	s_Device.Free();

	return false;
}

void GetSwapChainDesc1(DXGI_SWAP_CHAIN_DESC1& sd)
{
	// Found on forums, why BGRA is used here instead of RGBA:
	// Microsoft dude - "I believe there are some flip optimizatoin benefits to using BGR rather than RGB."
	// Update: So I changed the format to DXGI_FORMAT_R8G8B8A8_UNORM, checked the FPS in one of the games, didn't see any FPS loss
	//   But because all D3D11 gfxdevice was origanlly tested with DXGI_FORMAT_R8G8B8A8_UNORM, I think it makes sense to have this format instead
	//   It also makes image effects work correctly, for ex., it fixes bug - https://fogbugz.unity3d.com/default.asp?492440
	sd.Format = kSwapChainBackBufferFormat;
	sd.Stereo = FALSE;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = kSwapChainBackBufferCount;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	sd.Flags = kSwapChainFlags;
	sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
}
IDXGISwapChain1* CreateSwapChainForXAML(ISwapChainBackgroundPanelNative* panel, int width, int height)
{
	IDXGISwapChain1* chain;
	DXGI_SWAP_CHAIN_DESC1 sd;
	GetSwapChainDesc1(sd);

	sd.Width = width;
	sd.Height = height;

	HRESULT hr;
	sd.Scaling = DXGI_SCALING_STRETCH;
	hr = s_DXGIFactory->CreateSwapChainForComposition(s_Device, &sd, nullptr, &chain);
	DX_CHECK(hr, "CreateSwapChainForComposition failed with error 0x%08x", hr);

#if UNITY_METRO_VS2013
	UnityPlayer::AppCallbacks::Instance->InvokeOnUIThread(ref new UnityPlayer::AppCallbackItem([panel, chain]()
	{
		HRESULT hr = panel->SetSwapChain(chain);
		if (FAILED(hr)) FatalErrorMsg("SetSwapChain failed with error 0x%08x", hr);
	}
	), false);
#else
	hr = panel->SetSwapChain(chain);
	if (FAILED(hr)) FatalErrorMsg("SetSwapChain failed with error 0x%08x", hr);
#endif

	return chain;
error:
	return NULL;
}
IDXGISwapChain1* CreateSwapChainForD3D(IUnknown* coreWindow, int width, int height)
{
	IDXGISwapChain1* chain;
	DXGI_SWAP_CHAIN_DESC1 sd;
	GetSwapChainDesc1(sd);

	sd.Width = width;
	sd.Height = height;

	HRESULT hr;

	sd.Scaling = DXGI_SCALING_STRETCH;
	hr = s_DXGIFactory->CreateSwapChainForCoreWindow(s_Device, coreWindow, &sd, nullptr, &chain);
	DX_CHECK(hr, "CreateSwapChainForCoreWindow failed with error 0x%08x", hr);

	return chain;
error:
	return NULL;
}
bool InitializeOrResetD3D11SwapChain(GfxDevice* device, IDXGISwapChain1* chain, int width, int height, int vsynccount, int& outBackbufferBPP, int& outFrontbufferBPP, int& outDepthBPP, int& outFSAA)
{
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencil;

	//

	Assert(nullptr != device);
	Assert(nullptr != s_Device);
	Assert(nullptr != s_Context);
	Assert(nullptr != s_DXGIFactory);

	//

	outBackbufferBPP = 4;
	outFrontbufferBPP = 4;
	outDepthBPP = 4;
	outFSAA = 0;


	SAFE_RELEASE(s_DepthStencil.m_Texture);
	SAFE_RELEASE(s_DepthStencil.m_SRView);
	SAFE_RELEASE(s_DepthStencil.m_DSView);
	s_BackBuffer.Reset();

	//

	s_Context->OMSetRenderTargets(0, nullptr, nullptr);
	s_Context->Flush();

	s_SyncInterval = vsynccount;

	if (nullptr == s_SwapChain)
	{
		s_SwapChain = chain;
#if ENABLE_DX11_FRAME_LATENCY_WAITABLE_OBJECT
		s_FrameLatencyWaitableObject = s_EnableLowLatencyPresentationAPI ?
			((IDXGISwapChain2*) s_SwapChain)->GetFrameLatencyWaitableObject() :
			NULL;
#endif
	}
	else
	{
		// Use efficient DX 11.2 swap chain scaling if available
#if 0 && UNITY_METRO_VS2013

		// Check if new resolution fits into already allocated swap chain
		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		s_SwapChain->GetDesc(&swapChainDesc);
		if (width <= swapChainDesc.BufferDesc.Width && height <= swapChainDesc.BufferDesc.Height)
		{
			hr = ((IDXGISwapChain2*)s_SwapChain)->SetSourceSize(width, height);
			DX_CHECK(hr, "SetSourceSize failed with error 0x%08x", hr);
		}
		else
#endif
		{
			// Note: If you set third parameter as DXGI_FORMAT_UNKNOWN, D3D11 debugger incorrectly captures frame when swap chain is resized, for ex., when you perform snapping
			hr = s_SwapChain->ResizeBuffers(kSwapChainBackBufferCount, width, height, kSwapChainBackBufferFormat, kSwapChainFlags);
			DX_CHECK(hr, "ResizeBuffers failed with error 0x%08x", hr);
		}
	}

	hr = s_SwapChain->GetBuffer(0, __uuidof(backBuffer), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
	DX_CHECK(hr, "s_SwapChain->GetBuffer failed with error 0x%08x", hr);

	D3D11_TEXTURE2D_DESC backBufferDesc;
	backBuffer->GetDesc(&backBufferDesc);

	width = backBufferDesc.Width;
	height = backBufferDesc.Height;

	s_BackBuffer.m_Texture = backBuffer.Detach();
	s_BackBuffer.width = width;
	s_BackBuffer.height = height;
	s_BackBuffer.format = kRTFormatARGB32;

	device->SetCurrentWindowSize(width, height);

	ID3D11RenderTargetView* rtv = 0;
	hr = s_Device->CreateRenderTargetView(s_BackBuffer.m_Texture, nullptr, &rtv);
	DX_CHECK(hr, "CreateRenderTargetView failed with error 0x%08x", hr);
	s_BackBuffer.SetRTV(0,0,false,rtv);

	hr = s_Device->CreateTexture2D(&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL), nullptr, &depthStencil);
	DX_CHECK(hr, "CreateTexture2D failed with error 0x%08x", hr);

	device->SetFramebufferDepthFormat(kDepthFormat24);
	if (IsGfxDevice())
		GetGfxDevice().SetFramebufferDepthFormat(kDepthFormat24);

	s_DepthStencil.m_Texture = depthStencil.Get();

	hr = s_Device->CreateDepthStencilView(depthStencil.Get(), &CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D), &s_DepthStencil.m_DSView);	// ???
	DX_CHECK(hr, "CreateDepthStencilView failed with error 0x%08x", hr);

	s_BackBuffer.backBuffer = true;
	s_DepthStencil.backBuffer = true;

	ActivateD3D11BackBuffer(device);

	return true;

error:

	SAFE_RELEASE(s_DepthStencil.m_Texture);
	SAFE_RELEASE(s_DepthStencil.m_SRView);
	SAFE_RELEASE(s_DepthStencil.m_DSView);
	s_BackBuffer.Reset();

	SAFE_RELEASE(s_SwapChain);
#if UNITY_METRO_VS2013
	SAFE_RELEASE(s_DXGIDevice3);
#endif
	return false;
}

void ActivateD3D11BackBuffer(GfxDevice* device)
{
	Assert(nullptr != device);
	device->SetRenderTargets(1, &RenderSurfaceHandle(&s_BackBuffer), RenderSurfaceHandle(&s_DepthStencil));
}

void DestroyD3D11Device()
{
	// This can happen when quiting from screen selector - window is not set up yet
	if( !s_Device || !s_Context || !s_DXGIFactory)
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
#if UNITY_METRO_VS2013
	SAFE_RELEASE(s_DXGIDevice3);
#endif
	s_Context->ClearState();
	s_Context->Flush();

	s_Context->Release();
	s_Context = NULL;
	s_Device->Release();
	s_Device = NULL;
	s_DXGIFactory->Release();
	s_DXGIFactory = NULL;
}

void CleanupD3D11()
{
	AssertIf (s_Device || s_Context);
}

ID3D11Device* GetD3D11Device()
{
	AssertIf( !s_Device );
	return s_Device;
}

ID3D11Device1* GetD3D11_1Device()
{
	AssertIf( !s_Device );
	return s_Device;
}


ID3D11DeviceContext* GetD3D11Context(bool expectNull)
{
	if (!expectNull)
		Assert( s_Context );
	return s_Context;
}
#if ENABLE_DX11_FRAME_LATENCY_WAITABLE_OBJECT
HANDLE GetFrameLatencyWaitableObject()
{
	return s_FrameLatencyWaitableObject;
}
void WaitOnSwapChain()
{
	if (s_EnableLowLatencyPresentationAPI)
	{
		DWORD result = WaitForSingleObjectEx(
			s_FrameLatencyWaitableObject,
			1000, // 1 second timeout (shouldn't ever occur)
			true
			);
	}
}
#endif

#if UNITY_METRO_VS2013
IDXGIDevice3* GetIDXGIDevice3()
{
	return s_DXGIDevice3;
}
#endif
#endif
