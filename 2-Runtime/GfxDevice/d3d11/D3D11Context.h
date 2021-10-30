#pragma once

#include "D3D11Includes.h"

#if UNITY_METRO
#include <windows.ui.xaml.media.dxinterop.h>
#endif

struct D3D11Compiler;

bool InitializeD3D11();
void CleanupD3D11();
#if UNITY_WP8
HRESULT UpdateD3D11Device(ID3D11Device1* device, ID3D11DeviceContext1* deviceContext, ID3D11RenderTargetView* renderTargetView, int& width, int& height);
void ActivateD3D11BackBuffer(class GfxDevice* device);
#elif UNITY_METRO
IDXGISwapChain1* CreateSwapChainForXAML(ISwapChainBackgroundPanelNative* panel, int width, int height);
#if UNITY_METRO_VS2013
IDXGIDevice3* GetIDXGIDevice3();
#endif
IDXGISwapChain1* CreateSwapChainForD3D(IUnknown* coreWindow, int width, int height);
bool InitializeOrResetD3D11SwapChain(
	class GfxDevice* device,
	IDXGISwapChain1* chain, int width, int height, int vsynccount,
	int& outBackbufferBPP, int& outFrontbufferBPP, int& outDepthBPP, int& outFSAA);
void ActivateD3D11BackBuffer(class GfxDevice* device);
#else
bool InitializeOrResetD3D11SwapChain(
	class GfxDevice* device,
	HWND window, int width, int height,
	int refreshRate, bool fullscreen, int vsynccount, int fsaa,
	int& outBackbufferBPP, int& outFrontbufferBPP, int& outDepthBPP, int& outFSAA);
void ResizeSwapD3D11SwapChain (class GfxDevice* device, HWND window, int width, int height);

#endif

typedef std::vector<D3D_FEATURE_LEVEL> SupportedFeatureLevels;
SupportedFeatureLevels GetSupportedFeatureLevels();

void DestroyD3D11Device();

ID3D11Device* GetD3D11Device();
ID3D11Device1* GetD3D11_1Device();
ID3D11DeviceContext* GetD3D11Context(bool expectNull = false);

IDXGIFactory* GetDXGIFactory();
IDXGISwapChain* GetD3D11SwapChain();
int GetD3D11SyncInterval();

#if ENABLE_DX11_FRAME_LATENCY_WAITABLE_OBJECT
HANDLE GetFrameLatencyWaitableObject();
void WaitOnSwapChain();
#endif

extern ID3D11RenderTargetView* g_D3D11CurrRT;
extern ID3D11DepthStencilView* g_D3D11CurrDS;
struct RenderColorSurfaceD3D11;
struct RenderDepthSurfaceD3D11;
extern RenderColorSurfaceD3D11* g_D3D11CurrColorRT;
extern RenderDepthSurfaceD3D11* g_D3D11CurrDepthRT;

typedef int (WINAPI* D3D11PERF_BeginEventFunc)(DWORD, LPCWSTR);
typedef int (WINAPI* D3D11PERF_EndEventFunc)();
extern D3D11PERF_BeginEventFunc g_D3D11BeginEventFunc;
extern D3D11PERF_EndEventFunc g_D3D11EndEventFunc;
