#include "UnityPrefix.h"
#include "IGfxDevice.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Misc/Plugins.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Shaders/Shader.h"

static IGfxDevice* gGfxDevice;

IGfxDevice* GetIGfxDevice ()
{
	return gGfxDevice;
}

void SetIGfxDevice (IGfxDevice* manager)
{
	gGfxDevice = manager;
}


void CommonReloadResources(int flags)
{
	if (flags & GfxDevice::kReloadTextures)
		Texture::ReloadAll();

	if (flags & GfxDevice::kReloadShaders)
		Shader::ReloadAllShaders();

	if (flags & GfxDevice::kReleaseRenderTextures)
		RenderTexture::ReleaseAll();
}

bool InitializeGfxDevice() {
	if (!GetIGfxDevice()->InitializeGfxDevice())
		return false;
	
	GfxDevice& device = GetGfxDevice();
	device.SetMarkerCallback (PluginsRenderMarker);
	device.SetSetNativeGfxDeviceCallback (PluginsSetGraphicsDevice);
	device.SetReloadCallback (CommonReloadResources);
	return true;
}

void ParseGfxDeviceArgs () { return GetIGfxDevice()->ParseGfxDeviceArgs(); }
bool				IsGfxDevice() { return GetIGfxDevice()->IsGfxDevice(); }
GfxDevice&			GetGfxDevice() { return GetIGfxDevice()->GetGfxDevice(); }
GfxDevice&			GetUncheckedGfxDevice() { return GetIGfxDevice()->GetUncheckedGfxDevice(); }
void				DestroyGfxDevice() { GetIGfxDevice()->DestroyGfxDevice(); }

GfxDevice&			GetRealGfxDevice() { return GetIGfxDevice()->GetRealGfxDevice(); }
bool				IsRealGfxDeviceThreadOwner()  { return GetIGfxDevice()->IsRealGfxDeviceThreadOwner(); }

GraphicsCaps&		gGraphicsCaps { return GetIGfxDevice()->gGraphicsCaps; }
