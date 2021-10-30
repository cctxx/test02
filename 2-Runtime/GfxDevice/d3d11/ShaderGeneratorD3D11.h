#pragma once

#include "D3D11Includes.h"
#include "GpuProgramsD3D11.h"


struct FixedFunctionStateD3D11;

void* BuildVertexShaderD3D11 (const FixedFunctionStateD3D11& state, FixedFunctionProgramD3D11::ValueParameters& params, BuiltinShaderParamIndices& matrices, size_t& outSize);
void* BuildFragmentShaderD3D11 (const FixedFunctionStateD3D11& state, FixedFunctionProgramD3D11::ValueParameters& params, size_t& outSize);
