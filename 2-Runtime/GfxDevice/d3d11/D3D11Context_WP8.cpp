#include "UnityPrefix.h"

#if UNITY_WP8

#include "D3D11Context.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "D3D11Includes.h"
#include "D3D11Utils.h"
#include "TexturesD3D11.h"
#include "TimerQueryD3D11.h"
#include "Runtime/Misc/Plugins.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Utilities/LogUtility.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Graphics/ScreenManager.h"

SupportedFeatureLevels GetSupportedFeatureLevels()
{
	SupportedFeatureLevels features;

	features.push_back(D3D_FEATURE_LEVEL_9_3);

	return features;
}

#if ENABLE_PROFILER
D3D11PERF_BeginEventFunc g_D3D11BeginEventFunc = NULL;
D3D11PERF_EndEventFunc g_D3D11EndEventFunc = NULL;
#endif

using namespace Microsoft::WRL;


static ComPtr<ID3D11Device1> s_Device;
static ComPtr<ID3D11DeviceContext1> s_Context;

static RenderColorSurfaceD3D11 s_BackBuffer;
static RenderDepthSurfaceD3D11 s_DepthStencil;

ID3D11RenderTargetView* g_D3D11CurrRT;
ID3D11DepthStencilView* g_D3D11CurrDS;
ID3D11Resource* g_D3D11CurrRTResource;
ID3D11Resource* g_D3D11CurrDSResource;
RenderColorSurfaceD3D11* g_D3D11CurrColorRT;
RenderDepthSurfaceD3D11* g_D3D11CurrDepthRT;


HRESULT UpdateD3D11Device(ID3D11Device1* device, ID3D11DeviceContext1* deviceContext, ID3D11RenderTargetView* renderTargetView, int& width, int& height)
{
	HRESULT hr;

	Assert(device);

	// update device context

	if (deviceContext)
		s_Context = deviceContext;
	else
		device->GetImmediateContext1(s_Context.ReleaseAndGetAddressOf());

	// update device

	if (s_Device.Get() != device)
	{
		s_Context->OMSetRenderTargets(0, nullptr, nullptr);
		s_BackBuffer.Reset();
		s_DepthStencil.Reset();

		//@TODO: have to recreate all graphics resources,
		// Runtime/GfxDevice/GfxDeviceRecreate.h
		//
		// CleanupAllGfxDeviceResources should be called while the "old device" is still active;
		// RecreateAllGfxDeviceResources should be called when the "new device" is ready.
		//
		// Right now that works fine (tm) for switching editor between dx9 & dx11 in player settings,
		// but possibly needs some adjustment for WP8.

		s_Device = device;
		PluginsSetGraphicsDevice(device, kGfxRendererD3D11, kGfxDeviceEventInitialize);

		void RecreateAllGfxDeviceResources();

		if (deviceContext)
			RecreateAllGfxDeviceResources();
	}

	// update render target

	if (renderTargetView)
	{
		// get back buffer

		ComPtr<ID3D11Texture2D> backBuffer;

		{
			ComPtr<ID3D11Resource> resource;
			renderTargetView->GetResource(resource.GetAddressOf());

			hr = resource.As(&backBuffer);
			Assert(SUCCEEDED(hr));
		}

		// get description

		D3D11_TEXTURE2D_DESC backBufferDesc;
		backBuffer->GetDesc(&backBufferDesc);

		// get actual resolution

		width = backBufferDesc.Width;
		height = backBufferDesc.Height;

		bool const sizeChanged = ((s_BackBuffer.width != backBufferDesc.Width) || (s_BackBuffer.height != backBufferDesc.Height));

		// update back buffer

		s_Context->OMSetRenderTargets(0, nullptr, nullptr);
		Assert(DXGI_FORMAT_B8G8R8A8_UNORM == backBufferDesc.Format);

		s_BackBuffer.Reset();
		s_BackBuffer.width = backBufferDesc.Width;
		s_BackBuffer.height = backBufferDesc.Height;
		s_BackBuffer.backBuffer = true;
		s_BackBuffer.format = kRTFormatARGB32;
		s_BackBuffer.backBuffer = true;
		s_BackBuffer.m_Texture = backBuffer.Detach();

		s_BackBuffer.SetRTV(0, 0, false, renderTargetView);
		renderTargetView->AddRef();

		// update depth/stencil buffer if sice changed

		if (sizeChanged)
		{
			s_DepthStencil.Reset();

			// create depth/stencil texture

			ComPtr<ID3D11Texture2D> depthStencil;

			hr = s_Device->CreateTexture2D(&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, backBufferDesc.Width, backBufferDesc.Height, 1, 1, D3D11_BIND_DEPTH_STENCIL), nullptr, &depthStencil);
			Assert(SUCCEEDED(hr));

			if (FAILED(hr))
				return hr;

			// create depth/stencil view

			hr = s_Device->CreateDepthStencilView(depthStencil.Get(), &CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D), &s_DepthStencil.m_DSView);	// ???
			Assert(SUCCEEDED(hr));

			if (FAILED(hr))
				return hr;

			// store depth/stencil buffer

			s_DepthStencil.m_Texture = depthStencil.Detach();
			s_DepthStencil.width = backBufferDesc.Width;
			s_DepthStencil.height = backBufferDesc.Height;
			s_DepthStencil.backBuffer = true;	// ?!-
			s_DepthStencil.depthFormat = kDepthFormat24;

			GetGfxDevice().SetFramebufferDepthFormat(kDepthFormat24);
		}

		// set active render target

		ActivateD3D11BackBuffer(&GetGfxDevice());	// ?!-
	}

	return S_OK;
}

bool InitializeD3D11()
{
	Assert(s_Device);
	Assert(s_Context);
 	return true;
}

void CleanupD3D11()
{
	Assert(!s_Device);
	Assert(!s_Context);
}

void ActivateD3D11BackBuffer(GfxDevice* device)
{
	Assert(nullptr != device);
	device->SetRenderTargets(1, &RenderSurfaceHandle(&s_BackBuffer), RenderSurfaceHandle(&s_DepthStencil));
}

void DestroyD3D11Device()
{
	if (!s_Device || !s_Context)
		return;

	SAFE_RELEASE(s_DepthStencil.m_Texture);
	SAFE_RELEASE(s_DepthStencil.m_SRView);
	SAFE_RELEASE(s_DepthStencil.m_DSView);
	s_BackBuffer.Reset();

	s_Context->ClearState();
	s_Context->Flush();

	s_Context.Reset();
	s_Device.Reset();
}

ID3D11Device* GetD3D11Device()
{
	AssertIf(!s_Device);
	return s_Device.Get();
}

ID3D11Device1* GetD3D11_1Device()
{
	AssertIf( !s_Device );
	return s_Device.Get();
}

ID3D11DeviceContext* GetD3D11Context(bool expectNull)
{
	if (!expectNull)
		Assert(s_Context);
	return s_Context.Get();
}

#endif
