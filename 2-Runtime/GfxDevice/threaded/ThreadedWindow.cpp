#include "UnityPrefix.h"
#include "Runtime/GfxDevice/threaded/ThreadedWindow.h"
#include "Runtime/GfxDevice/threaded/GfxDeviceClient.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Misc/QualitySettings.h"


#if UNITY_WIN && UNITY_EDITOR

int	ThreadedWindow::ms_CurrentFSAALevel = 0;

ThreadedWindow::ThreadedWindow(HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias )
:	GfxDeviceWindow(window, width, height, depthFormat, antiAlias)
{
	m_ClientWindow = new ClientDeviceWindow;
	m_FSAALevel = antiAlias;
	m_Reshaped = false;

	// Creating the actual window calls Reshape on the base class
	// Threaded window should be kept in the same state
	GfxDeviceWindow::Reshape(width, height, depthFormat, antiAlias);
}

ThreadedWindow::~ThreadedWindow()
{
	GfxDeviceClient& device = (GfxDeviceClient&)GetGfxDevice();
	device.WindowDestroy(m_ClientWindow);
	m_ClientWindow = NULL;
}

bool ThreadedWindow::Reshape( int width, int height, DepthBufferFormat depthFormat, int antiAlias )
{
	if(!GfxDeviceWindow::Reshape(width, height, depthFormat, antiAlias))
		return false;

	GfxDeviceClient& device = (GfxDeviceClient&)GetGfxDevice();
	device.WindowReshape(m_ClientWindow, width, height, depthFormat, antiAlias);
	m_Reshaped = true;
	return true;
}

void ThreadedWindow::SetAsActiveWindow ()
{
	GfxDeviceClient& device = (GfxDeviceClient&)GetGfxDevice();
	device.SetActiveWindow(m_ClientWindow);
	OnActivateWindow();
}

bool ThreadedWindow::BeginRendering()
{
	if (GfxDeviceWindow::BeginRendering())
	{
		GfxDeviceClient& device = (GfxDeviceClient&)GetGfxDevice();
		device.BeginRendering(m_ClientWindow);
		OnActivateWindow();
		return true;
	}
	else
	{
		return false;
	}
}

bool ThreadedWindow::EndRendering( bool presentContent )
{
	if(GfxDeviceWindow::EndRendering(presentContent))
	{
		GfxDeviceClient& device = (GfxDeviceClient&)GetGfxDevice();
		device.EndRendering(m_ClientWindow, presentContent);
		if (m_Reshaped)
		{
			GfxDeviceRenderer renderer = device.GetRenderer();
			// We need to complete rendering on WM_PAINT after window was resized
			// otherwise contents will look stretched in DirectX mode
			if (renderer == kGfxRendererD3D9 || renderer == kGfxRendererD3D11)
				device.FinishRendering();
			m_Reshaped = false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

void ThreadedWindow::OnActivateWindow()
{
	GfxDeviceClient& device = (GfxDeviceClient&)GetGfxDevice();
	device.SetActiveRenderTexture(NULL);
	device.SetCurrentWindowSize(m_Width, m_Height);
	device.SetInvertProjectionMatrix(false);
	ms_CurrentFSAALevel = m_FSAALevel;

}

#endif
