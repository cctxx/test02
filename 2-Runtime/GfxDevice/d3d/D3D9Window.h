#ifndef D3D9WINDOW_H
#define D3D9WINDOW_H

#include "D3D9Includes.h"
#include "Runtime/GfxDevice/GfxDeviceWindow.h"
#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "D3D9Utils.h"
#include "TexturesD3D9.h"

class D3D9Window : public GfxDeviceWindow
{
private:
	IDirect3DDevice9*		m_Device;
	IDirect3DSwapChain9*	m_SwapChain;
	RenderColorSurfaceD3D9	m_BackBuffer;
	RenderDepthSurfaceD3D9	m_DepthStencil;
	D3DFORMAT				m_DepthStencilFormat;
	int						m_FSAALevel;
public:
	D3D9Window( IDirect3DDevice9* device, HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias );
	~D3D9Window();

	bool				Reshape( int width, int height, DepthBufferFormat depthFormat, int antiAlias );

	bool				BeginRendering();
	bool				EndRendering( bool presentContent );
	void				SetAsActiveWindow ();

	D3DFORMAT			GetDepthStencilFormat() const { return m_DepthStencilFormat; }

	RenderSurfaceHandle GetBackBuffer();
	RenderSurfaceHandle GetDepthStencil();
};

#if UNITY_EDITOR
int GetCurrentD3DFSAALevel();
#endif

#endif
