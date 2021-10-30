#pragma once

#include <string>
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

bool PatchVertexShaderFogD3D9 (std::string& src);
bool PatchPixelShaderFogD3D9 (std::string& src, FogMode fog, int fogColorReg, int fogParamsReg);
