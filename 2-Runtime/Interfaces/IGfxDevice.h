#ifndef IGFXDEVICE_H
#define IGFXDEVICE_H

class GfxDevice;
class GraphicsCaps;
class GpuProgramParameters;

class IGfxDevice
{
public:
	virtual bool IsGfxDevice() = 0;
	virtual bool InitializeGfxDevice() = 0;
	virtual bool IsRealGfxDeviceThreadOwner() = 0;
	virtual GfxDevice &GetGfxDevice() = 0;
	virtual GfxDevice &GetUncheckedGfxDevice() = 0;
	virtual GfxDevice &GetRealGfxDevice() = 0;
	virtual void DestroyGfxDevice() = 0;
	virtual void ParseGfxDeviceArgs() = 0;
	virtual GpuProgramParameters* CreateGpuProgramParameters() = 0;
	virtual void DestroyGpuProgramParameters(GpuProgramParameters*) = 0;
	virtual GraphicsCaps &gGraphicsCaps = 0;

};

EXPORT_COREMODULE IGfxDevice*	GetIGfxDevice ();
EXPORT_COREMODULE void			SetIGfxDevice (IGfxDevice* device);

#endif