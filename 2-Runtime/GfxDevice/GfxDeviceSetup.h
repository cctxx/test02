#pragma once

#include "GfxDeviceTypes.h"
class GfxDevice;

bool InitializeGfxDevice();

bool InitializeGfxDeviceWorkerProcess(size_t size, void *buffer);

#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
GfxDevice* CreateRealGfxDevice (GfxDeviceRenderer renderer, bool forceRef);
#endif
bool IsThreadableGfxDevice (GfxDeviceRenderer renderer);
void ParseGfxDeviceArgs ();

#define ENABLE_FORCE_GFX_RENDERER ((UNITY_WIN && !UNITY_WP8) || UNITY_LINUX || UNITY_ANDROID)

#if ENABLE_FORCE_GFX_RENDERER
extern GfxDeviceRenderer g_ForcedGfxRenderer;
#if GFX_SUPPORTS_D3D9
extern bool g_ForceD3D9RefDevice;
#endif
#endif

extern GfxThreadingMode g_ForcedGfxThreadingMode;


#if GFX_SUPPORTS_OPENGLES20 || GFX_SUPPORTS_OPENGLES30
extern int gDefaultFBO;
#endif
