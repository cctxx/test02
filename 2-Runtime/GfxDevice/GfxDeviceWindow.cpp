#include "UnityPrefix.h"
#include "GfxDeviceWindow.h"


GfxDeviceWindow::GfxDeviceWindow (NativeWindow window, int width, int height, DepthBufferFormat depthFormat, int antiAlias)
	: m_Window (window)
	, m_Width (0)
	, m_Height (0)
	, m_InvalidState (true)
	, m_CanUseBlitOptimization (false)
{
	//Reshape (width, height, depthFormat, antiAlias);
}

GfxDeviceWindow::~GfxDeviceWindow ()
{
}

bool GfxDeviceWindow::Reshape (int width, int height, DepthBufferFormat depthFormat, int antiAlias)
{
	m_InvalidState = false;

	AssertIf (!m_Window);

	m_Width = width;
	m_Height = height;

	if (m_Width <= 0 || m_Height <= 0)
	{
		m_InvalidState = true;
	}

	return !m_InvalidState;
}


bool GfxDeviceWindow::BeginRendering ()
{
	if (m_InvalidState)
	{
		return false;
	}

	return true;
}

bool GfxDeviceWindow::EndRendering (bool presentContent)
{
	if (m_InvalidState)
	{
		return false;
	}

	return true;
}
