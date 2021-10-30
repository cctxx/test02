#include "UnityPrefix.h"
#include "D3D11Window.h"
#include "D3D11Context.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Misc/QualitySettings.h"


#if UNITY_EDITOR

static D3D11Window*	s_CurrentD3D11Window = NULL;
static int			s_CurrentD3D11AA = 0;

void SetNoRenderTextureActiveEditor(); // RenderTexture.cpp
void InternalDestroyRenderSurfaceD3D11 (RenderSurfaceD3D11* rs, TexturesD3D11* textures); // RenderTextureD3D11.cpp
bool InitD3D11RenderDepthSurface (RenderDepthSurfaceD3D11& rs, TexturesD3D11* textures, bool sampleOnly);


D3D11Window::D3D11Window (HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias)
:	GfxDeviceWindow(window, width, height, depthFormat, antiAlias)
,	m_SwapChain(NULL)
,	m_AntiAlias(0)
{
	Reshape (width, height, depthFormat, antiAlias);
}

D3D11Window::~D3D11Window()
{
	if (s_CurrentD3D11Window == this)
	{
		s_CurrentD3D11Window = NULL;
		s_CurrentD3D11AA = 0;
	}

	InternalDestroyRenderSurfaceD3D11 (&m_DepthStencil, NULL);
	InternalDestroyRenderSurfaceD3D11 (&m_BackBuffer, NULL);
	SAFE_RELEASE (m_SwapChain);
}

bool D3D11Window::Reshape (int width, int height, DepthBufferFormat depthFormat, int antiAlias)
{
	if (!GfxDeviceWindow::Reshape(width, height, depthFormat, antiAlias))
		return false;

	// release old
	SAFE_RELEASE(m_DepthStencil.m_Texture);
	SAFE_RELEASE(m_DepthStencil.m_SRView);
	SAFE_RELEASE(m_DepthStencil.m_DSView);
	m_BackBuffer.Reset();
	SAFE_RELEASE(m_SwapChain);

	// pick supported AA level
	if (antiAlias == -1)
		antiAlias = GetQualitySettings().GetCurrent().antiAliasing;
	while (antiAlias > 1 && !(gGraphicsCaps.d3d11.msaa & (1<<antiAlias)))
		--antiAlias;
	antiAlias = std::max(antiAlias, 1);

	HRESULT hr;

	const bool sRGB = GetActiveColorSpace() == kLinearColorSpace;

	const bool createSRV = (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0); // swap chain can't have SRV before 10.0, it seems

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory (&sd, sizeof(DXGI_SWAP_CHAIN_DESC));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = m_Width;
	sd.BufferDesc.Height = m_Height;
	sd.BufferDesc.Format = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 0;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	if (createSRV)
		sd.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
	sd.OutputWindow = m_Window;
	sd.SampleDesc.Count = antiAlias;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	hr = GetDXGIFactory()->CreateSwapChain (GetD3D11Device(), &sd, &m_SwapChain);
	Assert(SUCCEEDED(hr));

	if (FAILED(hr))
	{
		printf_console ("d3d11: swap chain: w=%i h=%i fmt=%i\n",
			sd.BufferDesc.Width, sd.BufferDesc.Height, sd.BufferDesc.Format);
		printf_console ("d3d11: failed to create swap chain [0x%x]\n", hr);
		m_InvalidState = true;
		return !m_InvalidState;
	}

	//@TODO: false if AA is used?
	m_CanUseBlitOptimization = true;
	m_AntiAlias = 0;

	// Swap Chain
	ID3D11Texture2D* backBufferTexture;
	hr = m_SwapChain->GetBuffer (0, __uuidof(*backBufferTexture), (void**)&backBufferTexture);
	Assert(SUCCEEDED(hr));

	// Set the primary backbuffer view
	ID3D11RenderTargetView* rtView;
	D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
	rtDesc.Format = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
	rtDesc.ViewDimension = antiAlias > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
	rtDesc.Texture2D.MipSlice = 0;
	hr = GetD3D11Device()->CreateRenderTargetView (backBufferTexture, &rtDesc, &rtView);
	Assert(SUCCEEDED(hr));

	// Set the secondary backbuffer view
	ID3D11RenderTargetView* rtViewSecondary;
	D3D11_RENDER_TARGET_VIEW_DESC rtDescSecondary;
	rtDescSecondary.Format = !sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
	rtDescSecondary.ViewDimension = antiAlias > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
	rtDescSecondary.Texture2D.MipSlice = 0;
	hr = GetD3D11Device()->CreateRenderTargetView (backBufferTexture, &rtDescSecondary, &rtViewSecondary);
	Assert(SUCCEEDED(hr));

	// Create shader resource view
	if (createSRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = sd.BufferDesc.Format;
		srvDesc.ViewDimension = antiAlias > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		ID3D11ShaderResourceView* srView = NULL;
		hr = GetD3D11Device()->CreateShaderResourceView (backBufferTexture, &srvDesc, &m_BackBuffer.m_SRView);
		Assert (SUCCEEDED(hr));
	}

	//Pass through flags
	m_BackBuffer.m_Texture = backBufferTexture;
	m_BackBuffer.SetRTV (0, 0, false, rtView);
	m_BackBuffer.SetRTV (0, 0, true, rtViewSecondary);
	m_BackBuffer.width = sd.BufferDesc.Width;
	m_BackBuffer.height = sd.BufferDesc.Height;
	m_BackBuffer.samples = antiAlias;
	m_BackBuffer.format = kRTFormatARGB32;
	m_BackBuffer.backBuffer = true;
	m_BackBuffer.flags = sRGB ? (m_BackBuffer.flags | kSurfaceCreateSRGB) : (m_BackBuffer.flags & ~kSurfaceCreateSRGB);

	// Depth stencil
	m_DepthStencil.width = m_Width;
	m_DepthStencil.height = m_Height;
	m_DepthStencil.samples = antiAlias;
	m_DepthStencil.depthFormat = depthFormat;
	m_DepthStencil.backBuffer = true;

	bool dsOk = InitD3D11RenderDepthSurface (m_DepthStencil, NULL, false);
	Assert (dsOk);

	return !m_InvalidState;
}

void D3D11Window::SetAsActiveWindow ()
{
	GfxDevice& device = GetRealGfxDevice();
	device.SetRenderTargets(1, &GetBackBuffer(), GetDepthStencil());
	device.SetActiveRenderTexture(NULL);
	device.SetCurrentWindowSize(m_Width, m_Height);
	device.SetInvertProjectionMatrix(false);

	s_CurrentD3D11Window = this;
	s_CurrentD3D11AA = m_AntiAlias;
}

bool D3D11Window::BeginRendering()
{
	if (!GfxDeviceWindow::BeginRendering())
		return false;

	SetAsActiveWindow ();

	GfxDevice& device = GetRealGfxDevice();
	if (device.IsInsideFrame())
	{
		ErrorString ("GUI Window tries to begin rendering while something else has not finished rendering! Either you have a recursive OnGUI rendering, or previous OnGUI did not clean up properly.");
	}
	device.SetInsideFrame(true);

	return true;
}

bool D3D11Window::EndRendering( bool presentContent )
{
	if (!GfxDeviceWindow::EndRendering(presentContent))
		return false;

	GfxDevice& device = GetRealGfxDevice();
	Assert(device.IsInsideFrame());
	device.SetInsideFrame(false);

	s_CurrentD3D11Window = NULL;

	HRESULT hr;
	if (m_SwapChain && presentContent)
	{
		HRESULT hr = m_SwapChain->Present (0, 0);
		Assert (SUCCEEDED(hr));
	}
	return true;
}

RenderSurfaceHandle D3D11Window::GetBackBuffer()
{
	RenderSurfaceHandle handle;
	handle.object = &m_BackBuffer;
	return handle;
}

RenderSurfaceHandle D3D11Window::GetDepthStencil()
{
	RenderSurfaceHandle handle;
	handle.object = &m_DepthStencil;
	return handle;
}

#endif
