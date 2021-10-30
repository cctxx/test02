#include "UnwrapImpl.h"
#include "GeometryToolboxImpl.h"
#include "External/Wodka/wodka_WinHelper.h"
#include "External/Wodka/wodka_PELoader.h"
#include "External/Wodka/wodka_KnownImports.h"

#include <string>
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"


#if UNWRAPIMPL_USE_WODKA_DLL

#ifndef UNWRAPIMPL_USE_DLL_WINAPI
#define UNWRAPIMPL_USE_DLL_WINAPI 0 && UNITY_WIN
#endif


#if UNWRAPIMPL_USE_DLL_WINAPI
    static HMODULE      _UnwrapDll = 0;
#else
    static PEModule*    _UnwrapDll = 0;
#endif

typedef bool (*_GenerateSecondaryUVSetImpl)(const float*, SInt32, const float*, const float*, const UInt32*, SInt32, float*, const UnwrapParam&, char*, UInt32);
static _GenerateSecondaryUVSetImpl GenerateSecondaryUVSetImpl;

#else // UNWRAPIMPL_USE_WODKA_DLL

    #include "External/Unwrap/include/Unwrap.hpp"

#endif

extern PEKnownImport kGeometryToolboxKnownImports[];
extern int kGeometryToolboxKnownImportsCount;

namespace UnwrapImpl
{

void
LoadDll()
{
#if UNWRAPIMPL_USE_WODKA_DLL
    if(_UnwrapDll == 0)
    {
        std::string path = AppendPathName(GetApplicationContentsPath(), "Tools/Unwrap.dll");

        PEKnownImport* gtImport;
        unsigned gtImportCount;
        GeometryToolboxImpl::KnownImports(&gtImport, &gtImportCount);

    #if UNWRAPIMPL_USE_DLL_WINAPI
        _UnwrapDll = LoadLibraryA (path.c_str());
    #else
        _UnwrapDll = PELoadLibrary (path.c_str(), gtImport, gtImportCount);
    #endif

        if( _UnwrapDll != 0 )
        {
        #if UNWRAPIMPL_USE_DLL_WINAPI
            GenerateSecondaryUVSetImpl = (_GenerateSecondaryUVSetImpl)GetProcAddress(_UnwrapDll, "GenerateSecondaryUVSet");
        #else
            GenerateSecondaryUVSetImpl = (_GenerateSecondaryUVSetImpl)PEGetProcAddress(_UnwrapDll, "GenerateSecondaryUVSet");
        #endif
        }
    }
#endif
}

bool GenerateSecondaryUVSet( const float* vertex, SInt32 vertexCount,
                             const float* triNormal, const float* triUV, const UInt32* triangleList, SInt32 triangleCount,
                             float* outputUV, const UnwrapParam& param,
                             char* errorBuffer, UInt32 bufferSize
                           )
{
#if UNWRAPIMPL_USE_WODKA_DLL
    PESetupFS ();
    LoadDll();
    return GenerateSecondaryUVSetImpl(vertex, vertexCount, triNormal, triUV, triangleList, triangleCount, outputUV, param, errorBuffer, bufferSize);
#else
    return ::GenerateSecondaryUVSet(vertex, vertexCount, triNormal, triUV, triangleList, triangleCount, outputUV, param, errorBuffer, bufferSize);
#endif
}

} // namespace namespace UnwrapImpl
