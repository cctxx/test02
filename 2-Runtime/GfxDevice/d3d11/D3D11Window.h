#pragma once

#include "External/DirectX/builds/dx11include/d3d11.h"
#include "Runtime/GfxDevice/GfxDeviceWindow.h"
#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "TexturesD3D11.h"

class D3D11Window : public GfxDeviceWindow
{
private:
	IDXGISwapChain*			m_SwapChain;
	RenderColorSurfaceD3D11	m_BackBuffer;
	RenderDepthSurfaceD3D11	m_DepthStencil;
	int						m_AntiAlias;

public:
	D3D11Window (HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias);
	~D3D11Window ();

	bool				Reshape (int width, int height, DepthBufferFormat depthFormat, int antiAlias);

	bool				BeginRendering ();
	bool				EndRendering (bool presentContent);
	void				SetAsActiveWindow ();

	RenderSurfaceHandle GetBackBuffer();
	RenderSurfaceHandle GetDepthStencil();
};
