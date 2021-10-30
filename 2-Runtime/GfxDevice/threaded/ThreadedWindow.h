#ifndef THREADEDWINDOW_H
#define THREADEDWINDOW_H

#if UNITY_WIN && UNITY_EDITOR

#include "Runtime/GfxDevice/GfxDeviceWindow.h"
#include "ThreadedDeviceStates.h"

class ThreadedWindow : public GfxDeviceWindow
{
public:
	ThreadedWindow( HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias );
	~ThreadedWindow();

	bool				Reshape( int width, int height, DepthBufferFormat depthFormat, int antiAlias );

	bool				BeginRendering();
	bool				EndRendering( bool presentContent );
	void				SetAsActiveWindow();

	static int			GetCurrentFSAALevel() { return ms_CurrentFSAALevel; }

private:
	void				OnActivateWindow();

	friend class GfxDeviceClient;
	friend class GfxDeviceWorker;

	ClientDeviceWindow* m_ClientWindow;
	int				m_FSAALevel;
	bool			m_Reshaped;
	static int		ms_CurrentFSAALevel;
};

#endif
#endif
