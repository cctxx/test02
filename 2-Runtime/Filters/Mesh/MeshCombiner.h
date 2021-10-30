#ifndef MESHCOMBINER_H
#define MESHCOMBINER_H

#include "LodMesh.h"

class Renderer;

struct CombineInstance
{
	Mesh *mesh;
	int subMeshIndex;
	Matrix4x4f transform;
	
	Vector4f lightmapTilingOffset;
	int	vertexOffset;
	
	CombineInstance() :
		mesh(NULL),
		subMeshIndex(0),
		lightmapTilingOffset(1, 1, 0, 0),
		vertexOffset(0)
	{}
};

typedef std::vector<CombineInstance>	CombineInstances;

void CombineMeshes (const CombineInstances &in, Mesh& out, bool mergeSubMeshes, bool useTransforms);
// takes an array of meshes(their vertex data) and merges them into 1 combined mesh.
void CombineMeshVerticesForStaticBatching ( const CombineInstances& in, const string& combinedMeshName, Mesh& outCombinedMesh, bool useTransforms = true );
// takes an array of meshes(their indices) and merges them in 1 mesh (setups subsets) 
void CombineMeshIndicesForStaticBatching (const CombineInstances& in, Mesh& inoutMesh, bool mergeSubMeshes, bool useVertexOffsets);

#endif
