#include "UnityPrefix.h"
#include "ExtractDataFromMesh.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Misc/MeshWelding.h"
#include "Runtime/Graphics/TriStripper.h"

///@TODO: Use dynamic_array temp allocator
// Extracts unique welded vertices & triangle array indexing into the welded vertices.
bool ExtractDataFromMesh (Mesh& mesh, dynamic_array<Vector3f>& vertices, dynamic_array<UInt16>& triangles, dynamic_array<UInt16>& remap)
{
	int vertexCount = mesh.GetVertexCount();
	if (vertexCount == 0)
		return false;

	if (!mesh.HasVertexData())
	{
		ErrorStringObject("CollisionMeshData couldn't be created because the mesh has been marked as non-accessible", &mesh);
		return false;
	}
	
    vertices.resize_uninitialized(vertexCount);
	mesh.ExtractVertexArray(&vertices[0]);
		
    triangles.clear();
    for (unsigned submesh=0; submesh<mesh.GetSubMeshCount(); submesh++)
    {
        if (submesh >= mesh.GetSubMeshCount())
        {
            ErrorString("Failed getting triangles. Submesh index is out of bounds.");
            return false;
        }
        
		UInt16* indices = mesh.GetSubMeshBuffer16(submesh);
		SubMesh& sm = mesh.GetSubMeshFast(submesh);
		if (sm.topology == kPrimitiveTriangleStripDeprecated)
        {
			const UInt32 startIndex = triangles.size();
			int triCount = CountTrianglesInStrip (indices, sm.indexCount);
			triangles.resize_uninitialized((triCount * 3) + startIndex);
            Destripify(indices, sm.indexCount, triangles.begin() + startIndex, triCount);
        }
        else if (sm.topology == kPrimitiveTriangles)
		{
            triangles.insert(triangles.end(), indices, indices + sm.indexCount);
		}
		else
		{
			ErrorString("Failed to extract collision data: non-triangle mesh.");
			return false;
		}
    }

    WeldVertexArray(vertices, triangles, remap);
	return true;
}