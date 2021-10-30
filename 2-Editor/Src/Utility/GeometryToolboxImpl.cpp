#include "GeometryToolboxImpl.h"
#include "External/Wodka/wodka_WinHelper.h"
#include "External/Wodka/wodka_PELoader.h"
#include "External/Wodka/wodka_KnownImports.h"

#include <string>
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"


#if GTIMPL_USE_WODKA_DLL

#ifndef GTIMPL_USE_DLL_WINAPI
#define GTIMPL_USE_DLL_WINAPI 0 && UNITY_WIN
#endif

#if GTIMPL_USE_DLL_WINAPI
    static HMODULE      _GeometryToolboxDll = 0;
#else
    static PEModule*    _GeometryToolboxDll = 0;
#endif


typedef bool (*_GeometryToolbox_TestImpl)();
static _GeometryToolbox_TestImpl GeometryToolbox_TestMathBasicImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestTrigonometryImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestLogImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestMatrix4InvertImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestSparseMatrixImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestTriMapBasicImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestTriMapVertexIndexImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestCollapseEdgeBasicImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestSplitConnectedImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestNonManifoldFixerImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestDegenerateRemovalImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestArrayImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestBlobArrayImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestHashTabImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestPriorityQueueImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestProgressiveMeshInternalCollapseEdgeImpl = 0;
static _GeometryToolbox_TestImpl GeometryToolbox_TestSlidingWindowSimplePatchGenerateImpl = 0;

#define DECL_FUNC(Name, RetType, ArgsDecl)      \
typedef RetType (*_ ## Name ## Impl) ArgsDecl;  \
static _ ## Name ## Impl  Name ## Impl = 0      \


DECL_FUNC(GeometryToolbox_Allocate, void*, (unsigned));
DECL_FUNC(GeometryToolbox_Deallocate, void, (void*));
DECL_FUNC(GeometryToolbox_DestroyMeshDesc, void, (GeometryToolboxImpl::GeometryToolbox_MeshDesc*));
DECL_FUNC(GeometryToolbox_CleanupGeometry, void, (GeometryToolboxImpl::GeometryToolbox_MeshDesc*, GeometryToolboxImpl::GeometryToolbox_CleanupGeometryResult*));
DECL_FUNC(GeometryToolbox_FitIntoCube, void, (unsigned,float*,const float*,float));
DECL_FUNC(GeometryToolbox_CreateProgressiveMesh, GeometryToolboxImpl::GeometryToolbox_ProgressiveMesh, (const GeometryToolboxImpl::GeometryToolbox_MeshDesc&));
DECL_FUNC(GeometryToolbox_DestroyProgressiveMesh, void, (GeometryToolboxImpl::GeometryToolbox_ProgressiveMesh));
DECL_FUNC(GeometryToolbox_PMGenerateGeometry, void, (GeometryToolboxImpl::GeometryToolbox_ProgressiveMesh, GeometryToolboxImpl::GeometryToolbox_MeshDesc*));
DECL_FUNC(GeometryToolbox_PMUpdateTriangles, void, (GeometryToolboxImpl::GeometryToolbox_ProgressiveMesh, unsigned, GeometryToolboxImpl::GeometryToolbox_MeshDesc*));


#if GTIMPL_USE_DLL_WINAPI
    #define INIT_FUNC(Name, Type) Name##Impl = (Type)GetProcAddress( _GeometryToolboxDll, #Name );
#else
    #define INIT_FUNC(Name, Type) Name##Impl = (Type)PEGetProcAddress( _GeometryToolboxDll, #Name );
#endif

#define IMPL_FUNC(Name, RetType, ArgsDecl, ArgsPass) RetType Name ArgsDecl {PESetupFS(); LoadDll(); return Name##Impl ArgsPass ;}
#define IMPL_VOID_FUNC(Name, ArgsDecl, ArgsPass)     void Name ArgsDecl {PESetupFS(); LoadDll(); Name##Impl ArgsPass ;}


#else // GTIMPL_USE_WODKA_DLL

    #include "External/GeometryToolbox/include/GeometryToolbox.hpp"
    #include "External/GeometryToolbox/include/GeometryToolboxTest.hpp"

#define IMPL_FUNC(Name, RetType, ArgsDecl, ArgsPass) RetType Name ArgsDecl { return ::Name ArgsPass ;}
#define IMPL_VOID_FUNC(Name, ArgsDecl, ArgsPass)     void Name ArgsDecl { return ::Name ArgsPass ;}

#endif


namespace GeometryToolboxImpl
{

void
LoadDll()
{
#if GTIMPL_USE_WODKA_DLL
    if(_GeometryToolboxDll == 0)
    {
        std::string path = AppendPathName(GetApplicationContentsPath(), "Tools/GeometryToolbox.dll");

    #if GTIMPL_USE_DLL_WINAPI
        _GeometryToolboxDll = LoadLibrary (path.c_str());
    #else
        _GeometryToolboxDll = PELoadLibrary (path.c_str(), 0, 0);
    #endif


        if( _GeometryToolboxDll != 0 )
        {
            INIT_FUNC(GeometryToolbox_Allocate, _GeometryToolbox_AllocateImpl);
            INIT_FUNC(GeometryToolbox_Deallocate, _GeometryToolbox_DeallocateImpl);
            INIT_FUNC(GeometryToolbox_DestroyMeshDesc, _GeometryToolbox_DestroyMeshDescImpl);
            INIT_FUNC(GeometryToolbox_CleanupGeometry, _GeometryToolbox_CleanupGeometryImpl);
            INIT_FUNC(GeometryToolbox_FitIntoCube, _GeometryToolbox_FitIntoCubeImpl);
            INIT_FUNC(GeometryToolbox_CreateProgressiveMesh, _GeometryToolbox_CreateProgressiveMeshImpl);
            INIT_FUNC(GeometryToolbox_DestroyProgressiveMesh, _GeometryToolbox_DestroyProgressiveMeshImpl);
            INIT_FUNC(GeometryToolbox_PMGenerateGeometry, _GeometryToolbox_PMGenerateGeometryImpl);
            INIT_FUNC(GeometryToolbox_PMUpdateTriangles, _GeometryToolbox_PMUpdateTrianglesImpl);

            INIT_FUNC(GeometryToolbox_TestMathBasic, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestTrigonometry, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestLog, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestMatrix4Invert, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestSparseMatrix, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestTriMapBasic, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestTriMapVertexIndex, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestCollapseEdgeBasic, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestSplitConnected, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestNonManifoldFixer, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestDegenerateRemoval, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestArray, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestBlobArray, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestHashTab, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestPriorityQueue, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestProgressiveMeshInternalCollapseEdge, _GeometryToolbox_TestImpl);
            INIT_FUNC(GeometryToolbox_TestSlidingWindowSimplePatchGenerate, _GeometryToolbox_TestImpl);
        }
    }
#endif
}


IMPL_FUNC(GeometryToolbox_Allocate, void*, (unsigned sz), (sz));
IMPL_VOID_FUNC(GeometryToolbox_Deallocate, (void* ptr), (ptr));
IMPL_VOID_FUNC(GeometryToolbox_DestroyMeshDesc, (GeometryToolboxImpl::GeometryToolbox_MeshDesc* mesh), (mesh));
IMPL_VOID_FUNC(GeometryToolbox_CleanupGeometry, (GeometryToolboxImpl::GeometryToolbox_MeshDesc* srcMesh, GeometryToolboxImpl::GeometryToolbox_CleanupGeometryResult* out), (srcMesh, out));
IMPL_VOID_FUNC(GeometryToolbox_FitIntoCube, (unsigned posCount,float* pos,const float* center,float side), (posCount,pos,center,side));
IMPL_FUNC(GeometryToolbox_CreateProgressiveMesh, GeometryToolboxImpl::GeometryToolbox_ProgressiveMesh, (const GeometryToolboxImpl::GeometryToolbox_MeshDesc& mesh), (mesh));
IMPL_VOID_FUNC(GeometryToolbox_DestroyProgressiveMesh, (GeometryToolboxImpl::GeometryToolbox_ProgressiveMesh mesh), (mesh));
IMPL_VOID_FUNC(GeometryToolbox_PMGenerateGeometry, (GeometryToolboxImpl::GeometryToolbox_ProgressiveMesh pm, GeometryToolboxImpl::GeometryToolbox_MeshDesc* outMesh), (pm, outMesh));
IMPL_VOID_FUNC(GeometryToolbox_PMUpdateTriangles, (GeometryToolboxImpl::GeometryToolbox_ProgressiveMesh pm, unsigned targetTriCount, GeometryToolboxImpl::GeometryToolbox_MeshDesc* outMesh), (pm,targetTriCount,outMesh));


IMPL_FUNC(GeometryToolbox_TestMathBasic, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestTrigonometry, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestLog, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestMatrix4Invert, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestSparseMatrix, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestTriMapBasic, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestTriMapVertexIndex, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestCollapseEdgeBasic, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestSplitConnected, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestNonManifoldFixer, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestDegenerateRemoval, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestArray, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestBlobArray, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestHashTab, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestPriorityQueue, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestProgressiveMeshInternalCollapseEdge, bool, (), ());
IMPL_FUNC(GeometryToolbox_TestSlidingWindowSimplePatchGenerate, bool, (), ());


void KnownImports(PEKnownImport** imports, unsigned* count)
{
    LoadDll();

#if GTIMPL_USE_WODKA_DLL
    #define IMP(f) { #f, (void*)f##Impl, 0 }
#else
    #define IMP(f) { #f, (void*)f, 0 }
#endif

    static PEKnownImport _KnownImports[] =
    {
        IMP(GeometryToolbox_Allocate),
        IMP(GeometryToolbox_Deallocate),
        IMP(GeometryToolbox_DestroyMeshDesc),
        IMP(GeometryToolbox_CleanupGeometry),
        IMP(GeometryToolbox_FitIntoCube),
        IMP(GeometryToolbox_CreateProgressiveMesh),
        IMP(GeometryToolbox_DestroyProgressiveMesh),
        IMP(GeometryToolbox_PMGenerateGeometry),
        IMP(GeometryToolbox_PMUpdateTriangles),
        IMP(GeometryToolbox_TestMathBasic),
        IMP(GeometryToolbox_TestTrigonometry),
        IMP(GeometryToolbox_TestLog),
        IMP(GeometryToolbox_TestMatrix4Invert),
        IMP(GeometryToolbox_TestSparseMatrix),
        IMP(GeometryToolbox_TestTriMapBasic),
        IMP(GeometryToolbox_TestTriMapVertexIndex),
        IMP(GeometryToolbox_TestCollapseEdgeBasic),
        IMP(GeometryToolbox_TestSplitConnected),
        IMP(GeometryToolbox_TestNonManifoldFixer),
        IMP(GeometryToolbox_TestDegenerateRemoval),
        IMP(GeometryToolbox_TestArray),
        IMP(GeometryToolbox_TestBlobArray),
        IMP(GeometryToolbox_TestHashTab),
        IMP(GeometryToolbox_TestPriorityQueue),
        IMP(GeometryToolbox_TestProgressiveMeshInternalCollapseEdge),
        IMP(GeometryToolbox_TestSlidingWindowSimplePatchGenerate)
    };
    static int _KnownImportsCount = sizeof(_KnownImports)/sizeof(_KnownImports[0]);

    *imports = _KnownImports;
    *count   = _KnownImportsCount;

    #undef IMP
}


} // namespace namespace GeometryToolboxImpl


#undef INIT_FUNC
#undef IMPL_VOID_FUNC
#undef IMPL_FUNC


