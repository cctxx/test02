#pragma once

#include "Runtime/GfxDevice/GfxDevice.h"

void SetAllowPlugins (bool allow);

// Never change the enum values!
// They are used in low level native plugin interface.
enum GfxDeviceEventType {
	kGfxDeviceEventInitialize = 0,
	kGfxDeviceEventShutdown = 1,
	kGfxDeviceEventBeforeReset = 2,
	kGfxDeviceEventAfterReset = 3,
};

#if UNITY_PLUGINS_AVAILABLE
const char* FindAndLoadUnityPlugin (const char* pluginName);
// Used by GfxDevice
void PluginsSetGraphicsDevice (void* device, int deviceType, GfxDeviceEventType eventType);
void PluginsRenderMarker (int marker);
void UnloadAllPlugins ();
#else
inline void PluginsSetGraphicsDevice (void* device, int deviceType, GfxDeviceEventType eventType) { }
inline void PluginsRenderMarker (int marker) { }
#endif
