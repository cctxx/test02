#pragma once


#if UNITY_WP8

#include <initguid.h>

#include <dxgi1_2.h>
#include <d3d11_1.h>

typedef struct _D3D11_SIGNATURE_PARAMETER_DESC
{
    LPCSTR                      SemanticName;   // Name of the semantic
    UINT                        SemanticIndex;  // Index of the semantic
    UINT                        Register;       // Number of member variables
    D3D_NAME                    SystemValueType;// A predefined system value, or D3D_NAME_UNDEFINED if not applicable
    D3D_REGISTER_COMPONENT_TYPE ComponentType;  // Scalar type (e.g. uint, float, etc.)
    BYTE                        Mask;           // Mask to indicate which components of the register
                                                // are used (combination of D3D10_COMPONENT_MASK values)
    BYTE                        ReadWriteMask;  // Mask to indicate whether a given component is 
                                                // never written (if this is an output signature) or
                                                // always read (if this is an input signature).
                                                // (combination of D3D10_COMPONENT_MASK values)
    UINT                        Stream;         // Stream index
    D3D_MIN_PRECISION           MinPrecision;   // Minimum desired interpolation precision
} D3D11_SIGNATURE_PARAMETER_DESC;

#elif UNITY_METRO

#include <dxgi1_2.h>
#include <d3d11_1.h>
#if UNITY_METRO_VS2013
#include <d3d11_2.h>
#endif
#include <d3d11shader.h>

#else

#include "External/DirectX/builds/dx11include/DXGI.h"
#include "External/DirectX/builds/dx11include/D3D11.h"
#include "External/DirectX/builds/dx11include/d3d11_1.h"
#include "External/DirectX/builds/dx11include/D3D11Shader.h"

#define D3D_NAME	D3D10_NAME
#define D3D_REGISTER_COMPONENT_TYPE	D3D10_REGISTER_COMPONENT_TYPE

#endif

#include "D3D11Debug.h"
