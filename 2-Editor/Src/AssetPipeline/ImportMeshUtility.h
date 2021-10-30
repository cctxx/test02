#pragma once
#include "ImportMesh.h"
class Mesh;

void SplitMesh (const ImportMesh& mesh, ImportMesh& splitMesh, float splitAngle = 1.0F);

/// Removes any unneccessary vertices from the mesh.
/// Attributes have to be in wedge format
void RemoveUnusedVertices (ImportMesh& mesh);

/// Returns an error if the mesh has out of bounds faces or degenerate triangles. (The Lodmesh will still be valid)
/// The input meshes normal, color and uv array are per face vertex (3 * triangle Count)
/// This function will then split & merge the vertices optimally
///
/// - Removes degenerate triangles (Two triangle indices are the same in a triangle. Does not check the area of the triangle)
/// - Corrects face winding (Walking through edges making sure that adjacent edges have the same winding!)
/// - Generates one face array per material if faceMaterialIndex is defined!
/// - Splits the mesh
/// - Creates tangent spaces
/// - Splits mesh (if any of attributes are over max<Uint32>)
/// - Generates mesh
class AssetImporter;
std::string GenerateMeshData (AssetImporter& assetImporter, const ImportMesh& constantMesh, const Matrix4x4f& transform, const ImportMeshSettings& settings, const std::string& lodMeshName, std::vector<Mesh*>& lodMesh, std::vector< std::vector<int> >& lodMeshMaterials, std::vector<std::string>& warnings);

void TransformMesh (ImportMesh& mesh, const Matrix4x4f& transform);


// It will correct the winding and corrects winding errors and remove degenerate triangles.
// is2Manifold returns true if the mesh always has less than 2 triangles connected on one edge.
// isClosed returns whether the mesh always has 2 triangles on one edge, which means that it is completely closed
void ConsolidateMesh (ImportMesh& mesh, bool* is2Manifold, bool* isClosed);

void RemoveDegenerateFaces (ImportMesh& mesh);

void WeldVertices (ImportMesh& mesh);

void InvertWinding(ImportMesh& mesh);

// will update vertices attributes from provided per-tri lists
// TODO: add new ones when needed
void InsertVertexAttr(Mesh& mesh, const Vector2f* uv, const Vector2f* uv2, const ColorRGBAf* color, const unsigned* triMaterial);