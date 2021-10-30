#ifndef DRAWUTIL_H
#define DRAWUTIL_H

#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Modules/ExportModules.h"

namespace Unity { class Material; }
class ChannelAssigns;
using namespace Unity;
class Vector2f;
class Vector3f;
class ColorRGBAf;
class Quaternionf;
class Mesh;
class VBO;
class Matrix4x4f;
class ComputeBuffer;
class Sprite;


enum MeshPrimitiveType
{
	kTriangles = 0,
	kLines = 1,
};

// Utilities for drawing
struct EXPORT_COREMODULE DrawUtil
{
	static void DrawVBOMeshRaw (VBO& vbo, Mesh& mesh, const ChannelAssigns& channels, int materialIndex, UInt32 channelsInVBO = ~0UL);

	/// Draw a mesh with the current matrix
	static void DrawMeshRaw (const ChannelAssigns& channels, Mesh &mesh, int materialIndex);
	
	static void DrawMesh (const ChannelAssigns& channels, Mesh &mesh, const Vector3f &position, const Quaternionf &rotation, int materialIndex = -1);
	static void DrawMesh (const ChannelAssigns& channels, Mesh &mesh, const Matrix4x4f &matrix, int materialIndex);

	static void DrawProcedural (GfxPrimitiveType topology, int vertexCount, int instanceCount);

	// args in ComputeBuffer at given offset:
	// uint32 vertexCountPerInstance
	// uint32 instanceCount
	// uint32 startVertexLocation
	// uint32 startInstanceLocation
	static void DrawProceduralIndirect (GfxPrimitiveType topology, ComputeBuffer* bufferWithArgs, UInt32 argsOffset);


	static void DrawSpriteRaw (const ChannelAssigns& channels, Sprite& sprite, const ColorRGBAf& color);
};

#endif
