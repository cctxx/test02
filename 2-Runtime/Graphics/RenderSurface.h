#pragma once

#include "Runtime/GfxDevice/GfxDeviceTypes.h"

struct
RenderSurfaceBase
{
	TextureID	textureID;
	int			width;
	int			height;
	int 		samples;
	UInt32		flags;
	bool		colorSurface;
	bool		backBuffer;
	bool		shouldDiscard;
	bool		shouldClear;
};

// we dont want to enforce ctor, so lets do it as simple function
inline void RenderSurfaceBase_Init(RenderSurfaceBase& rs)
{
	rs.textureID.m_ID = 0;
	rs.width = rs.height = 0;
	rs.samples = 1;
	rs.flags = 0;
	rs.shouldDiscard = rs.shouldClear = false;
	rs.backBuffer = false;
}

inline void RenderSurfaceBase_InitColor(RenderSurfaceBase& rs)
{
	RenderSurfaceBase_Init(rs);
	rs.colorSurface = true;
}

inline void RenderSurfaceBase_InitDepth(RenderSurfaceBase& rs)
{
	RenderSurfaceBase_Init(rs);
	rs.colorSurface = false;
}
