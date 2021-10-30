#ifndef UNITY_RENDER_TEXTURES_GLES20_H_
#define UNITY_RENDER_TEXTURES_GLES20_H_

#include "Configuration/UnityConfigure.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"
#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "IncludesGLES20.h"

RenderSurfaceHandle CreateRenderColorSurfaceGLES2 (TextureID textureID, unsigned rbID, int width, int height, TextureDimension dim, UInt32 createFlags, RenderTextureFormat format, int samples);
RenderSurfaceHandle CreateRenderDepthSurfaceGLES2 (TextureID textureID, unsigned rbID, int width, int height, UInt32 createFlags, DepthBufferFormat depthFormat, int samples);
void DestroyRenderSurfaceGLES2 (RenderSurfaceHandle& rsHandle);
bool SetRenderTargetGLES2 (int count, RenderSurfaceHandle* colorHandle, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, unsigned int globalSharedFBO);
RenderSurfaceHandle GetActiveRenderColorSurfaceGLES2 ();
RenderSurfaceHandle GetActiveRenderDepthSurfaceGLES2 ();
void GrabIntoRenderTextureGLES2 (RenderSurfaceHandle rs, RenderSurfaceHandle rd, int x, int y, int width, int height, GLuint globalSharedFBO, GLuint helperFBO);
void ResolveColorSurfaceGLES2(RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle, GLuint globalSharedFBO, GLuint helperFBO);
void DiscardContentsGLES2(RenderSurfaceHandle rs);

int GetCurrentFBGLES2();
void EnsureDefaultFBInitedGLES2();

RenderTextureFormat GetCurrentFBColorFormatGLES20();

void InitBackBufferGLES2(RenderSurfaceBase** color, RenderSurfaceBase** depth);
void SetBackBufferGLES2();

#if UNITY_IPHONE
void* UnityCreateExternalColorSurfaceGLES2(unsigned texid, unsigned rbid, int width, int height, bool is32bit);
void* UnityCreateExternalDepthSurfaceGLES2(unsigned texid, unsigned rbid, int width, int height, bool is24bit);
void  UnityDestroyExternalColorSurfaceGLES2(void* surf);
void  UnityDestroyExternalDepthSurfaceGLES2(void* surf);
#endif

#if UNITY_IPHONE
bool IsActiveMSAARenderTargetGLES2();
#endif


#endif
