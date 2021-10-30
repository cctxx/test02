#ifndef GFXDEVICEWINDOW_H
#define GFXDEVICEWINDOW_H

#include "GfxDeviceTypes.h"

class GfxDeviceWindow
{
protected:
	NativeWindow	m_Window;
	int		m_Width;
	int		m_Height;
	bool	m_InvalidState;
	bool	m_CanUseBlitOptimization;
public:
	GfxDeviceWindow (NativeWindow window, int width, int height, DepthBufferFormat depthFormat, int antiAlias);
	virtual ~GfxDeviceWindow();

	//Returns true if reshaping was successful
	virtual bool		Reshape( int width, int height, DepthBufferFormat depthFormat, int antiAlias );

	//Returns true if succeeded to prepare for rendering
	virtual bool		BeginRendering();

	virtual void		SetAsActiveWindow () { };

	//Returns true if succeeded to finish rendering
	virtual bool		EndRendering( bool presentContent );

	inline bool			CanUseBlitOptimization() const { return m_CanUseBlitOptimization; }

	inline int			GetWidth() const  { return m_Width; }
	inline int			GetHeight() const { return m_Height; }
	inline NativeWindow GetHandle () const { return m_Window; }
};

#endif
