#ifndef MESH_WELDING_H_
#define MESH_WELDING_H_

#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Modules/ExportModules.h"

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/ImportMesh.h"

void WeldVertices (ImportMesh& mesh);
#endif

bool EXPORT_COREMODULE WeldVertexArray(dynamic_array<Vector3f>& vertices, Mesh::BoneInfluenceContainer& skin, dynamic_array<UInt16>& triangles, dynamic_array<UInt16>& remap);  
bool EXPORT_COREMODULE WeldVertexArray(dynamic_array<Vector3f>& vertices, dynamic_array<UInt16>& triangles, dynamic_array<UInt16>& remap);  


#endif // UNITY_CUSTOM_ALLOCATOR_H_
