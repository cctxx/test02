#pragma once

#include "D3D9Includes.h"


RenderSurfaceHandle CreateRenderColorSurfaceD3D9 (TextureID textureID, int width, int height, int samples, TextureDimension dim, UInt32 createFlags, RenderTextureFormat format, TexturesD3D9& textures);
RenderSurfaceHandle CreateRenderDepthSurfaceD3D9 (TextureID textureID, int width, int height, int samples, DepthBufferFormat depthFormat, UInt32 createFlags, TexturesD3D9& textures);
void DestroyRenderSurfaceD3D9 (RenderSurfaceD3D9* rs);
void DestroyRenderSurfaceD3D9 (RenderSurfaceHandle& rsHandle, TexturesD3D9& textures);
bool SetRenderTargetD3D9 (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, int& outRenderTargetWidth, int& outRenderTargetHeight, bool& outIsBackBuffer);
RenderSurfaceHandle GetActiveRenderColorSurfaceD3D9(int index);
RenderSurfaceHandle GetActiveRenderDepthSurfaceD3D9();

RenderSurfaceHandle GetBackBufferColorSurfaceD3D9();
RenderSurfaceHandle GetBackBufferDepthSurfaceD3D9();
void SetBackBufferColorSurfaceD3D9(RenderSurfaceBase* color);
void SetBackBufferDepthSurfaceD3D9(RenderSurfaceBase* depth);
