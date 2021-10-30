#pragma once

#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Utilities/dynamic_array.h"

enum {
	k11FogColor = 0,
	k11FogParams = 1,
	k11FogSize = 2
};

enum {
	// DX11 has 14 constant buffers, and for safety reasons
	// let's use 3rd to last one, 11. If that is already
	// in use by shader, there will be no fog.
	k11FogConstantBufferBind = 11,
};

bool PatchVertexOrDomainShaderFogD3D11 (dynamic_array<UInt8>& byteCode);
bool PatchPixelShaderFogD3D11 (dynamic_array<UInt8>& byteCode, FogMode fog);

bool PatchRemovePartialPrecisionD3D11 (dynamic_array<UInt8>& byteCode);

bool PatchRemovePartialPrecisionD3D11 (dynamic_array<UInt8>& byteCode);
