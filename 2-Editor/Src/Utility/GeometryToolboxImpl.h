#ifndef _EDITOR_UTILITY_GEOMETRYTOOLBOXIMPL_H
#define _EDITOR_UTILITY_GEOMETRYTOOLBOXIMPL_H

struct PEKnownImport;

#ifndef GTIMPL_USE_WODKA_DLL
#define GTIMPL_USE_WODKA_DLL 1
#endif

#if !GTIMPL_USE_WODKA_DLL
#include "External/GeometryToolbox/include/GeometryToolbox.hpp"
#endif

namespace GeometryToolboxImpl
{
    void LoadDll();
    void KnownImports(PEKnownImport** imports, unsigned* count);

    void*   GeometryToolbox_Allocate(unsigned sz);
    void    GeometryToolbox_Deallocate(void* ptr);


#if GTIMPL_USE_WODKA_DLL
    struct
    GeometryToolbox_MeshDesc
    {
        unsigned    posCount;
        float*      pos;

        unsigned    triCount;
        unsigned*   tri;

        // per-triangle attributes
        unsigned*   triMaterial;

        // vertex attributes
        // usually input is per triangle apex, output per vertex (vb contents)
        float*      normal;
        float*      uv;
        float*      uv2;
        float*      color;
    };
#else
    typedef ::GeometryToolbox_MeshDesc GeometryToolbox_MeshDesc;
#endif

#if GTIMPL_USE_WODKA_DLL
    struct
    GeometryToolbox_CleanupGeometryResult
    {
        unsigned*   srcTriI;
        unsigned*   degTriI; // points inside srcTriI memory

        ~GeometryToolbox_CleanupGeometryResult()
        {
            GeometryToolbox_Deallocate(srcTriI);
        }
    };
#else
    typedef ::GeometryToolbox_CleanupGeometryResult GeometryToolbox_CleanupGeometryResult;
#endif

#if GTIMPL_USE_WODKA_DLL
    struct
    GeometryToolbox_ProgressiveMesh
    {
        void*   implOpaquePtr;
    };
#else
    typedef ::GeometryToolbox_ProgressiveMesh GeometryToolbox_ProgressiveMesh;
#endif


    void GeometryToolbox_DestroyMeshDesc(GeometryToolbox_MeshDesc* desc);
    void GeometryToolbox_CleanupGeometry(GeometryToolbox_MeshDesc* srcMesh, GeometryToolbox_CleanupGeometryResult* out);
    void GeometryToolbox_FitIntoCube(unsigned posCount, float* pos, const float* center, float side);

    GeometryToolbox_ProgressiveMesh  GeometryToolbox_CreateProgressiveMesh(const GeometryToolbox_MeshDesc& srcMesh);
    void                             GeometryToolbox_DestroyProgressiveMesh(GeometryToolbox_ProgressiveMesh pm);

    void GeometryToolbox_PMGenerateGeometry(GeometryToolbox_ProgressiveMesh pm, GeometryToolbox_MeshDesc* outMesh);
    void GeometryToolbox_PMUpdateTriangles(GeometryToolbox_ProgressiveMesh pm, unsigned targetTriCount, GeometryToolbox_MeshDesc* outMesh);


    bool GeometryToolbox_TestMathBasic();
    bool GeometryToolbox_TestTrigonometry();
    bool GeometryToolbox_TestLog();
    bool GeometryToolbox_TestMatrix4Invert();
    bool GeometryToolbox_TestSparseMatrix();

    bool GeometryToolbox_TestTriMapBasic();
    bool GeometryToolbox_TestTriMapVertexIndex();
    bool GeometryToolbox_TestCollapseEdgeBasic();

    bool GeometryToolbox_TestSplitConnected();

    bool GeometryToolbox_TestNonManifoldFixer();
    bool GeometryToolbox_TestDegenerateRemoval();

    bool GeometryToolbox_TestArray();
    bool GeometryToolbox_TestBlobArray();
    bool GeometryToolbox_TestHashTab();
    bool GeometryToolbox_TestPriorityQueue();

    bool GeometryToolbox_TestProgressiveMeshInternalCollapseEdge();
    bool GeometryToolbox_TestSlidingWindowSimplePatchGenerate();

};


#endif
