#pragma once

#include "GfxDeviceConfigure.h"
#include "Runtime/Camera/BaseRenderer.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Math/Color.h"

struct BatchInstanceData;
class Matrix4x4f;
struct SubMesh;
struct VertexBufferData;
class BaseRenderer;
class ChannelAssigns;
#if ENABLE_SPRITES
struct SpriteRenderData;
#endif
struct BatchRenderer
{
	dynamic_array<BatchInstanceData>	m_BatchInstances;
	ChannelAssigns const*	m_ActiveChannels;
	RendererType m_ActiveType;

	BatchRenderer();
	
	void Add(BaseRenderer* renderer, int subsetIndex, ChannelAssigns const* channels, const Matrix4x4f& xform, int xformType);
	void Flush();
};

void AppendMeshIndices(UInt16* dstIndices, size_t& dstIndexCount, const UInt16* srcIndices, size_t srcIndexCount, bool isTriStrip);
size_t TransformIndices(UInt16* dst, const void* srcIB, size_t firstByte, size_t indexCount, size_t firstVertex, size_t batchVertexOffset, bool isTriStrip);
size_t TransformVertices(UInt8* dst, Matrix4x4f const& m, const VertexBufferData& src, size_t firstVertex, size_t vertexCount, UInt32 channelsInVBO);
#if ENABLE_SPRITES
void TransformSprite (UInt8* vb, UInt16* ib, const Matrix4x4f* m, const SpriteRenderData* rd, ColorRGBA32 color, size_t firstVertex);
#endif
