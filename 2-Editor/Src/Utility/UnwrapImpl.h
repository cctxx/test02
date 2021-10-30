#ifndef _EDITOR_UTILITY_UNWRAP_H
#define _EDITOR_UTILITY_UNWRAP_H

#ifndef UNWRAPIMPL_USE_WODKA_DLL
#define UNWRAPIMPL_USE_WODKA_DLL 1
#endif

#include "External/Unwrap/include/UnwrapParam.hpp"

#if !UNWRAPIMPL_USE_WODKA_DLL
#include "External/Unwrap/include/Unwrap.hpp"
#endif

namespace UnwrapImpl
{
    void LoadDll();

    bool GenerateSecondaryUVSet( const float* vertex, SInt32 vertexCount,
                                 const float* triNormal, const float* triUV, const UInt32* triangleList, SInt32 triangleCount,
                                 float* outputUV, const UnwrapParam& param,
                                 char* errorBuffer=0, UInt32 bufferSize=0
                               );

};


#endif
