#pragma once

#include "RenderLoopEnums.h"

class Shader;
struct RenderLoop;
class Camera;
class ImageFilters;
class RenderTexture;
struct ShadowCullData;
struct CullResults;


RenderLoop* CreateRenderLoop (Camera& camera);
void DeleteRenderLoop (RenderLoop* loop);
void DoRenderLoop (
	RenderLoop& loop,
	RenderingPath renderPath,
	CullResults& contents,
	// used in the editor for material previews - those should not render projectors, halos etc.
	bool dontRenderRenderables
);
void CleanupAfterRenderLoop (RenderLoop& loop);
ImageFilters& GetRenderLoopImageFilters (RenderLoop& loop);
void RenderImageFilters (RenderLoop& loop, RenderTexture* targetTexture, bool afterOpaque);
