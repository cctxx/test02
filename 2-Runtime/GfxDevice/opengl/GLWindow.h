#ifndef GLWINDOW_H
#define GLWINDOW_H

#include "UnityGL.h"
#include "Runtime/GfxDevice/GfxDeviceWindow.h"
#include "GLContext.h"

class GLWindow : public GfxDeviceWindow
{
private:
	GraphicsContextHandle	m_GLContext;
	int						cc;
public:
	GLWindow (NativeWindow window, int width, int height, DepthBufferFormat depthFormat, int antiAlias);
	~GLWindow();

	bool				Reshape( int width, int height, DepthBufferFormat depthFormat, int antiAlias );

	bool				BeginRendering();
	bool				EndRendering( bool presentContent );

public:
	static GLWindow*	Current();


};


#endif
