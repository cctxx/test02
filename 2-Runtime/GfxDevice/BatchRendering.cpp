#include "UnityPrefix.h"
#include "BatchRendering.h"

#include "GfxDevice.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Filters/Mesh/TransformVertex.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Filters/Mesh/SpriteRenderer.h"
#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Utilities/Prefetch.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemRenderer.h"


BatchRenderer::BatchRenderer()
:m_BatchInstances(kMemTempAlloc)
,m_ActiveChannels(0)
,m_ActiveType(kRendererUnknown)
{
	m_BatchInstances.reserve(128);
}

void BatchRenderer::Add(BaseRenderer* renderer, int subsetIndex, ChannelAssigns const* channels, const Matrix4x4f& xform, int xformType)
{
	const RendererType rendererType = renderer->GetRendererType();
	if (m_ActiveChannels != channels || m_ActiveType != rendererType)
		Flush();

	m_ActiveChannels = channels;
	m_ActiveType = rendererType;

#if GFX_ENABLE_DRAW_CALL_BATCHING

	// batching requires position
	bool channelSetupAllowsBatching = (channels->GetTargetMap() & (1<<kVertexCompVertex));
#if GFX_OPENGLESxx_ONLY
	// we do not renormalize normals in gles11 so let's play safe here
	channelSetupAllowsBatching &= !(GetGfxDevice().IsPositionRequiredForTexGen() || GetGfxDevice().IsNormalRequiredForTexGen());
#endif


	bool rendererTypeAllowsBatching = (rendererType == kRendererMesh) || (rendererType == kRendererSprite);
	if(rendererType == kRendererParticleSystem)
	{
		ParticleSystemRenderer* psRenderer = (ParticleSystemRenderer*)renderer;
		if(kSRMMesh != psRenderer->GetRenderMode()) // We currently don't do batching for mesh particle emitters
			rendererTypeAllowsBatching = true;
	}
	if (channelSetupAllowsBatching && rendererTypeAllowsBatching)
	{
		BatchInstanceData& rb = m_BatchInstances.push_back();
		rb.renderer = (Renderer*)renderer;
		rb.subsetIndex = subsetIndex;
		rb.xform = xform;
		rb.xformType = xformType;
	}
	else
#endif
	{
		Assert(channels);
		SetupObjectMatrix (xform, xformType);
		renderer->Render (subsetIndex, *channels);
	}
}

void BatchRenderer::Flush()
{
	size_t instanceCount = m_BatchInstances.size();
#if GFX_ENABLE_DRAW_CALL_BATCHING
	if (instanceCount > 0)
	{
		Assert(m_ActiveChannels);
		if (instanceCount == 1)
		{
			const BatchInstanceData* data = m_BatchInstances.begin();
			SetupObjectMatrix (data->xform, data->xformType);
			data->renderer->Render (data->subsetIndex, *m_ActiveChannels);
		}
		else if(m_ActiveType == kRendererMesh)
			MeshRenderer::RenderMultiple (m_BatchInstances.begin(), instanceCount, *m_ActiveChannels);
		else if(m_ActiveType == kRendererParticleSystem)
			ParticleSystemRenderer::RenderMultiple (m_BatchInstances.begin(), instanceCount, *m_ActiveChannels);
#if ENABLE_SPRITES
		else if (m_ActiveType == kRendererSprite)
			SpriteRenderer::RenderMultiple (m_BatchInstances.begin(), instanceCount, *m_ActiveChannels);
#endif
		else
			Assert (!"Renderer type does not support batching");
	}
#else
	Assert(instanceCount == 0);
#endif
	m_BatchInstances.resize_uninitialized(0);
	m_ActiveChannels = 0;
	m_ActiveType = kRendererUnknown;
}

void AppendMeshIndices(UInt16* dstIndices, size_t& dstIndexCount, const UInt16* srcIndices, size_t srcIndexCount, bool isTriStrip)
{
	Prefetch(srcIndices, srcIndexCount * kVBOIndexSize);

	if (isTriStrip && dstIndexCount > 0)
	{
		dstIndices[dstIndexCount] = dstIndices[dstIndexCount - 1];
		dstIndexCount++;
		dstIndices[dstIndexCount] = srcIndices[0];
		dstIndexCount++;
	}

	memcpy(&dstIndices[dstIndexCount], srcIndices, srcIndexCount * kVBOIndexSize);
	dstIndexCount += srcIndexCount;

	if (isTriStrip && (srcIndexCount % 2 == 1))
	{
		Assert(dstIndexCount != 0);
		dstIndices[dstIndexCount] = dstIndices[dstIndexCount - 1];
		dstIndexCount++;
		Assert(dstIndexCount % 2 == 0);
	}
}


static inline void
TransformIndicesInternalImplPositive( int* __restrict dst, const int* __restrict src, unsigned count, int offset )
{
	Assert( offset>=0 );
	Assert( offset<(int)0xFFFF );
	const int maskOffset = ((unsigned)offset << 16) | (unsigned)offset;

	for( unsigned i = 0 ; i < count ; ++i )
		*dst++ = *src++ + maskOffset;
}

static inline void
TransformIndicesInternalImplNegative( int* __restrict dst, const int* __restrict src, unsigned count, int offset )
{
	Assert( offset>=0 );
	Assert( offset<(int)0xFFFF );
	const int maskOffset = ((unsigned)offset << 16) | (unsigned)offset;

	for( unsigned i = 0 ; i < count ; ++i )
		*dst++ = *src++ - maskOffset;
}

size_t TransformIndices(UInt16* dst, const void* srcIB, size_t firstByte, size_t indexCount, size_t firstVertex, size_t batchVertexOffset, bool isTriStrip)
{
	UInt16* srcIndices = (UInt16*)((UInt8*)srcIB + firstByte);
	Prefetch(srcIndices, indexCount * kVBOIndexSize);

	UInt16* const baseDataPtr = dst;
	if (isTriStrip && batchVertexOffset > 0)
	{
		Assert(srcIndices[0] >= firstVertex);
		dst[0] = dst[-1];
		dst[1] = srcIndices[0] - firstVertex + batchVertexOffset;
		dst += 2;
	}

	int offset		= (int)batchVertexOffset - (int)firstVertex;
	int copyCount	= indexCount/2;
	if( offset >= 0 )	TransformIndicesInternalImplPositive((int*)dst, (int*)srcIndices, copyCount, offset);
	else				TransformIndicesInternalImplNegative((int*)dst, (int*)srcIndices, copyCount, -offset);

	// leftover, as we copy ints
	if(2*copyCount != indexCount)
		dst[indexCount-1] = srcIndices[indexCount-1] - firstVertex + batchVertexOffset;

#ifdef CHECK_INDEX_TRANSFORM_RESULTS
	const UInt16* dstCheck = dst;
	for( unsigned i = 0 ; i < indexCount ; ++i )
		Assert( dstCheck[i] == srcIndices[i] - submesh.firstVertex + batchVertexOffset );
#endif

	dst += indexCount;
	if (isTriStrip && indexCount % 2 == 1)
	{
		dst[0] = dst[-1];
		++dst;
	}
	Assert ((dst - baseDataPtr) <= indexCount + 3);
	return (dst - baseDataPtr);
}

size_t TransformVertices(UInt8* dst, Matrix4x4f const& m, const VertexBufferData& src, size_t firstVertex, size_t vertexCount, UInt32 channelsInVBO)
{
	UInt8* inChannels[kShaderChannelCount];
	UInt32 inStrides[kShaderChannelCount];
	bool multiStream = false;
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		if (channelsInVBO & (1 << i))
		{
			const ChannelInfo& info = src.channels[i];
			DebugAssert(info.IsValid());
			UInt32 stride = info.CalcStride(src.streams);
			UInt32 offset = info.CalcOffset(src.streams) + stride * firstVertex;
			inChannels[i] = &src.buffer[offset];
			inStrides[i] = stride;
			if (info.stream > 0)
				multiStream = true;
		}
		else
		{
			inChannels[i] = NULL;
			inStrides[i] = 0;
		}
	}

	TransformVerticesStrided(
		StrideIterator<Vector3f>(inChannels[kShaderChannelVertex], inStrides[kShaderChannelVertex]),
		StrideIterator<Vector3f>(inChannels[kShaderChannelNormal], inStrides[kShaderChannelNormal]),
		StrideIterator<ColorRGBA32>(inChannels[kShaderChannelColor], inStrides[kShaderChannelColor]),
		StrideIterator<Vector2f>(inChannels[kShaderChannelTexCoord0], inStrides[kShaderChannelTexCoord0]),
		StrideIterator<Vector2f>(inChannels[kShaderChannelTexCoord1], inStrides[kShaderChannelTexCoord1]),
		StrideIterator<Vector4f>(inChannels[kShaderChannelTangent], inStrides[kShaderChannelTangent]),
		dst, m, vertexCount, multiStream);

	return vertexCount;
}
#if ENABLE_SPRITES
void TransformSprite (UInt8* vb, UInt16* ib, const Matrix4x4f* m, const SpriteRenderData* rd, ColorRGBA32 color, size_t firstVertex)
{
	UInt32  vertexCount = rd->vertices.size();
	UInt32  indexCount  = rd->indices.size();
	UInt8*  srcVertices = vertexCount > 0 ? (UInt8  *)&rd->vertices[0] : NULL;
	UInt16* srcIndices  = indexCount > 0 ? (UInt16 *)&rd->indices[0] : NULL;

#if (UNITY_SUPPORTS_NEON || UNITY_SUPPORTS_VFP) && !UNITY_DISABLE_NEON_SKINNING
	int stride = sizeof(SpriteVertex);
	UInt8* end = srcVertices + vertexCount * stride;
	UInt8* uv  = srcVertices + sizeof(Vector3f);
	Matrix4x4f xform = (m) ? *m : Matrix4x4f::identity;
#	if UNITY_SUPPORTS_NEON
	if (CPUInfo::HasNEONSupport())
		s_TransformVertices_Sprite_NEON(srcVertices, end, uv, xform.m_Data, (UInt8 *)vb, stride, color.GetUInt32());
	else
#	endif
#	if UNITY_SUPPORTS_VFP
		s_TransformVertices_Sprite_VFP (srcVertices, end, uv, xform.m_Data, (UInt8 *)vb, stride, color.GetUInt32());
#	else
	{
		ErrorString("non-NEON path not enabled!");
	}
#	endif
#else
	struct SpriteVBOLayout {
		Vector3f  pos;
		ColorRGBA32 col;
		Vector2f uv;
	};
	SpriteVertex    *src = (SpriteVertex *)srcVertices;
	SpriteVBOLayout *dst = (SpriteVBOLayout *)vb;
	while (vertexCount-- > 0)
	{
		dst->pos = (m) ? m->MultiplyPoint3(src->pos) : src->pos;
		dst->col = color;
		dst->uv  = src->uv;
		dst++;
		src++;
	}
#endif
	Prefetch(srcIndices, indexCount * kVBOIndexSize);
	if (firstVertex)
	{
		int maskOffset = ((unsigned)firstVertex << 16) | (unsigned)firstVertex;
		int copyCount  = indexCount>>1;

		if(2*copyCount != indexCount)
			ib[indexCount-1] = srcIndices[indexCount-1] + firstVertex;
		int *dst = (int *)ib;
		int *src = (int *)srcIndices;
		while (copyCount-- > 0)
			*(dst++) = *(src++) + maskOffset;
	}
	else
		memcpy(ib, srcIndices, sizeof(UInt16) * indexCount);
}
#endif
