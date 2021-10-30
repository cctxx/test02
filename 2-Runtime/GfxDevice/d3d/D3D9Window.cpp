#include "UnityPrefix.h"
#include "D3D9Window.h"
#include "GfxDeviceD3D9.h"
#include "RenderTextureD3D.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Threads/ThreadSharedObject.h"
#include "Runtime/GfxDevice/GfxDevice.h"


#if UNITY_EDITOR

bool IsD3D9DeviceLost();
void SetD3D9DeviceLost( bool lost );

static bool			s_OldHasDepthFlag = false;
static D3D9Window*	s_CurrentD3DWindow = NULL;
static int			s_CurrentD3DFSAALevel = 0;

int GetCurrentD3DFSAALevel() { return s_CurrentD3DFSAALevel; }

void SetNoRenderTextureActiveEditor(); // RenderTexture.cpp


D3D9Window::D3D9Window(IDirect3DDevice9* device, HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias )
:	GfxDeviceWindow(window, width, height, depthFormat, antiAlias)
,	m_SwapChain(NULL)
,	m_FSAALevel(0)
{
	m_Device = device;
	Reshape( width, height, depthFormat, antiAlias );
}

D3D9Window::~D3D9Window()
{
	if( s_CurrentD3DWindow == this )
	{
		s_CurrentD3DWindow = NULL;
		s_CurrentD3DFSAALevel = 0;
	}

	DestroyRenderSurfaceD3D9(&m_DepthStencil);
	DestroyRenderSurfaceD3D9(&m_BackBuffer);
	SAFE_RELEASE(m_SwapChain);
}

bool D3D9Window::Reshape( int width, int height, DepthBufferFormat depthFormat, int antiAlias )
{
	if(GfxDeviceWindow::Reshape(width, height, depthFormat, antiAlias)==false)return false;


	#if ENABLE_D3D_WINDOW_LOGGING
	printf_console("D3Dwindow %x Reshape %ix%i d=%i aa=%i\n", this, width, height, depthFormat, antiAlias);
	#endif
	// release old
	m_DepthStencil.Release();
	m_BackBuffer.Release();
	SAFE_RELEASE(m_SwapChain);

	HRESULT hr;


	// Choose presentation params
	if( antiAlias == -1 )
		antiAlias = GetQualitySettings().GetCurrent().antiAliasing;

	D3DDISPLAYMODE mode;
	hr = GetD3DObject()->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &mode );
	D3DPRESENT_PARAMETERS params;

	ZeroMemory( &params, sizeof(params) );
	params.BackBufferWidth = m_Width;
	params.BackBufferHeight = m_Height;
	params.BackBufferCount = 1;
	params.hDeviceWindow = m_Window;
	params.FullScreen_RefreshRateInHz = 0;
	params.Windowed = TRUE;
	params.SwapEffect = D3DSWAPEFFECT_COPY;
	params.BackBufferFormat = D3DFMT_A8R8G8B8;
	params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	params.EnableAutoDepthStencil = FALSE;
	GetD3DFormatCaps()->FindBestPresentationParams( width, height, mode.Format, true, 0, antiAlias, params );
	if( params.MultiSampleType != D3DMULTISAMPLE_NONE ) {
		params.SwapEffect = D3DSWAPEFFECT_DISCARD;
		m_CanUseBlitOptimization = false;
	} else {
		m_CanUseBlitOptimization = true;
	}
	m_FSAALevel = (params.MultiSampleType == D3DMULTISAMPLE_NONMASKABLE) ? params.MultiSampleQuality : params.MultiSampleType;

	hr = m_Device->CreateAdditionalSwapChain( &params, &m_SwapChain );
	if( FAILED(hr) ) {
		printf_console( "d3d: swap chain: swap=%i vsync=%x w=%i h=%i fmt=%i bbcount=%i dsformat=%i pflags=%x\n",
			params.SwapEffect, params.PresentationInterval,
			params.BackBufferWidth, params.BackBufferHeight, params.BackBufferFormat, params.BackBufferCount,
			params.AutoDepthStencilFormat, params.Flags );
		printf_console( "d3d: failed to create swap chain [%s]\n", GetD3D9Error(hr) );
		m_InvalidState = true;
		return !m_InvalidState;
	}

	IDirect3DSurface9* backBuffer = NULL;
	hr = m_SwapChain->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer );
	if( FAILED(hr) ) {
		AssertString( "Failed to get back buffer for D3DWindow" );
		m_SwapChain->Release();
		m_SwapChain = NULL;
		m_InvalidState = true;
		return !m_InvalidState;
	}

	m_BackBuffer.backBuffer		= true;
	m_DepthStencil.backBuffer	= true;

	m_BackBuffer.m_Surface = backBuffer;
	m_BackBuffer.width = params.BackBufferWidth;
	m_BackBuffer.height = params.BackBufferHeight;
	m_BackBuffer.format = kRTFormatARGB32;

	// Depth format
	bool needsDepth = false;
	m_DepthStencilFormat = D3DFMT_UNKNOWN;
	switch( depthFormat ) {
	case kDepthFormatNone:
		needsDepth = false;
		m_DepthStencilFormat = D3DFMT_UNKNOWN;
		break;
	case kDepthFormat16:
		needsDepth = true;
		m_DepthStencilFormat = D3DFMT_D16;
		break;
	case kDepthFormat24:
		needsDepth = true;
		m_DepthStencilFormat = D3DFMT_D24S8;
		break;
	default:
		ErrorString("Unknown depth format");
	}

	if( needsDepth )
	{
		D3D9DepthStencilTexture depthStencil = CreateDepthStencilTextureD3D9 (m_Device, m_Width, m_Height, m_DepthStencilFormat, params.MultiSampleType, params.MultiSampleQuality, FALSE);
		m_Device->SetRenderState (D3DRS_ZENABLE, TRUE);
		if (!depthStencil.m_Surface)
		{
			AssertString( "Failed to create depth/stencil for D3DWindow" );
			m_SwapChain->Release();
			m_SwapChain = NULL;
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_BackBuffer.m_Surface);
			m_BackBuffer.m_Surface->Release();
			m_BackBuffer.m_Surface = NULL;
			m_InvalidState = true;
			return !m_InvalidState;
		}
		m_DepthStencil.m_Surface = depthStencil.m_Surface;
		m_DepthStencil.m_Texture = depthStencil.m_Texture;
		m_DepthStencil.width = m_Width;
		m_DepthStencil.height = m_Height;
		m_DepthStencil.depthFormat = depthFormat;
	}

	return !m_InvalidState;
}

void D3D9Window::SetAsActiveWindow ()
{
	GetRealGfxDevice().SetRenderTargets(1, &GetBackBuffer(), GetDepthStencil());
	GetRealGfxDevice().SetActiveRenderTexture(NULL);
	GetRealGfxDevice().SetCurrentWindowSize(m_Width, m_Height);
	GetRealGfxDevice().SetInvertProjectionMatrix(false);

	s_OldHasDepthFlag = g_D3DHasDepthStencil;
	g_D3DHasDepthStencil = (m_DepthStencil.m_Surface != NULL);

	s_CurrentD3DWindow = this;
	s_CurrentD3DFSAALevel = m_FSAALevel;

	// not entirely correct but better not touch anything if we don't have depth
	if(m_DepthStencil.m_Surface != NULL)
		g_D3DDepthStencilFormat = m_DepthStencilFormat;
}

bool D3D9Window::BeginRendering()
{
	if (GfxDeviceWindow::BeginRendering())
	{
		HRESULT hr;

		// Handle lost devices
		if (!GetRealGfxDevice().IsValidState())
		{
			return false;
		}

		// begin scene
		if (IsD3D9DeviceLost())
		{
			ErrorString ("GUI Window tries to begin rendering while D3D9 device is lost!");
		}
		GfxDeviceD3D9& device = static_cast<GfxDeviceD3D9&>( GetRealGfxDevice() );
		if (device.IsInsideFrame())
		{
			ErrorString ("GUI Window tries to begin rendering while something else has not finished rendering! Either you have a recursive OnGUI rendering, or previous OnGUI did not clean up properly.");
		}

		m_Device->BeginScene();
		SetAsActiveWindow ();

		device.SetInsideFrame(true);
		return true;
	}
	else
	{
		#if ENABLE_D3D_WINDOW_LOGGING
		printf_console("D3Dwindow %ix%i BeginRendering: invalid state\n", m_Width, m_Height);
		#endif
		return false;
	}
}

bool D3D9Window::EndRendering( bool presentContent )
{
	if(GfxDeviceWindow::EndRendering(presentContent))
	{

		g_D3DHasDepthStencil = s_OldHasDepthFlag;
		s_CurrentD3DWindow = NULL;
		s_CurrentD3DWindow = 0;

		if(  IsD3D9DeviceLost() )
			return false;

		HRESULT hr;
		GfxDeviceD3D9& device = static_cast<GfxDeviceD3D9&>( GetRealGfxDevice() );
		Assert( device.IsInsideFrame() );
		hr = m_Device->EndScene();
		device.SetInsideFrame(false);
		if( m_SwapChain && presentContent )
		{
			hr = m_SwapChain->Present( NULL, NULL, NULL, NULL, 0 );
			device.PushEventQuery();
			// When D3DERR_DRIVERINTERNALERROR is returned from Present(),
			// the application can do one of the following, try recovering just as
			// from the lost device.
			if( hr == D3DERR_DEVICELOST || hr == D3DERR_DRIVERINTERNALERROR )
			{
				SetD3D9DeviceLost( true );
				return false;
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}

RenderSurfaceHandle D3D9Window::GetBackBuffer()
{
	RenderSurfaceHandle handle;
	handle.object = &m_BackBuffer;
	return handle;
}

RenderSurfaceHandle D3D9Window::GetDepthStencil()
{
	RenderSurfaceHandle handle;
	handle.object = &m_DepthStencil;
	return handle;
}

#endif
