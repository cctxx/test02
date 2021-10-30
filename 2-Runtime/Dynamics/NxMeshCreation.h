#pragma once

class Mesh;
class Matrix4x4f;
enum TransformType;
class MemoryStream;

bool CreateNxStreamFromUnityMesh (Mesh* mesh, bool convex, const Matrix4x4f& scalematrix, TransformType transformType, MemoryStream& stream );
MemoryStream* CreateNxStreamFromUnityMesh(Mesh& meshData, bool convex);
//static void* CreateNxMeshFromByteStream(bool convex,dynamic_array<UInt8>& mesh)
void* CreateNxMeshFromUnityMesh (Mesh* mesh, bool convex, const Matrix4x4f& scalematrix, TransformType transformType );
