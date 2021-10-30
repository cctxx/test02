#include "UnityPrefix.h"
#include "SpriteRenderer.h"

#if ENABLE_SPRITES

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Graphics/SpriteFrame.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Filters/Mesh/TransformVertex.h"
#include "Runtime/GfxDevice/BatchRendering.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/BaseClasses/Tags.h"
#include "SpriteRendererAnimationBinding.h"


PROFILER_INFORMATION(gSpriteRenderSingleProfile, "SpriteRenderer.RenderSingle", kProfilerRender)
PROFILER_INFORMATION(gSpriteRenderBatchProfile, "SpriteRenderer.RenderBatch", kProfilerRender)
PROFILER_INFORMATION(gSpriteRenderSubmitVBO, "Mesh.SubmitVBO", kProfilerRender)

const float kSpriteScaleEpsilon = 0.0001f;
#define kMaxNumSpriteTrianglesPerBatch (2*1024)

static const char* const kDefaultSpriteShader = "Sprites/Default";
static const char* const kDefaultSpriteMaterial = "Sprites-Default.mat";

static SHADERPROP (MainTex);
static SHADERPROP (MainTex_TexelSize);
static Material* gSpriteDefaultMaterial = NULL;

static void InitDefaultSpriteMaterial()
{
	Assert(gSpriteDefaultMaterial == NULL);
	gSpriteDefaultMaterial = GetBuiltinResource<Material>(kDefaultSpriteMaterial);
}

IMPLEMENT_CLASS_HAS_INIT (SpriteRenderer)
IMPLEMENT_OBJECT_SERIALIZE (SpriteRenderer)

SpriteRenderer::SpriteRenderer (MemLabelId label, ObjectCreationMode mode)
: Super(kRendererSprite, label, mode)
, m_Color(1.0F, 1.0F, 1.0F, 1.0F)
{
	m_CastShadows = false;
	m_ReceiveShadows = false;
}

SpriteRenderer::~SpriteRenderer ()
{
}

inline ColorRGBA32 GetDeviceColor (const ColorRGBAf& color, GfxDevice& device)
{
	if (GetActiveColorSpace () == kLinearColorSpace)
		return device.ConvertToDeviceVertexColor(GammaToActiveColorSpace(color));
	else
		return device.ConvertToDeviceVertexColor(color);
}

void SpriteRenderer::InitializeClass ()
{
	REGISTER_GLOBAL_CALLBACK(initializedEngineGraphics, InitDefaultSpriteMaterial());
	InitializeSpriteRendererAnimationBindingInterface();
}

void SpriteRenderer::CleanupClass ()
{
	CleanupSpriteRendererAnimationBindingInterface ();
	gSpriteDefaultMaterial = NULL;
}

template<class TransferFunction>
void SpriteRenderer::Transfer(TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	TRANSFER (m_Sprite);
	TRANSFER (m_Color);
}

void SpriteRenderer::UpdateLocalAABB ()
{
	if (m_Sprite.IsValid())
	{
		//TODO: calculate AABB from RenderData.
		m_TransformInfo.localAABB = m_Sprite->GetBounds();
	}
	else
	{
		m_TransformInfo.localAABB.SetCenterAndExtent(Vector3f::zero, Vector3f::zero);
	}
}

void SpriteRenderer::UpdateTransformInfo ()
{
	Transform const& transform = GetTransform();
	if (m_TransformDirty)
	{
		// will return a cached matrix most of the time
		TransformType type = transform.CalculateTransformMatrix (m_TransformInfo.worldMatrix);

		// Always treat sprites has having a non-uniform scale. Will make them batch better
		// (since we break batches on transform type changes). And does not have any negative effects
		// since uniform vs. non-uniform scale only affects fixed function vertex normals, which
		// aren't relevant here.
		type &= ~kUniformScaleTransform;
		type |= kNonUniformScaleTransform;
		m_TransformInfo.transformType = type;

		// Likewise, treat inverse scale as always being 1.
		m_TransformInfo.invScale = 1.0f;
	}

	if (m_BoundsDirty)
		UpdateLocalAABB();

	TransformAABBSlow(m_TransformInfo.localAABB, m_TransformInfo.worldMatrix, m_TransformInfo.worldAABB);
}

void SpriteRenderer::SetSprite(PPtr<Sprite> sprite)
{
	if (m_Sprite != sprite)
	{
		m_Sprite = sprite;
		BoundsChanged();
		SetupMaterialProperties();

		SetDirty();
	}
}

void SpriteRenderer::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad(awakeMode);
	BoundsChanged();
	SetupMaterialProperties();
}

void SpriteRenderer::SmartReset ()
{
	SetMaterialCount(1);
	SetMaterial(GetDefaultSpriteMaterial(), 0);
}

void SpriteRenderer::SetupMaterialProperties()
{
	if (m_Sprite.IsNull())
		return;

	// Patch sprite texture and apply material property block
	MaterialPropertyBlock& block = GetPropertyBlockRememberToUpdateHash ();
	SetupMaterialPropertyBlock(block, GetSpriteRenderDataInContext(m_Sprite)->texture);
	ComputeCustomPropertiesHash ();
}

void SpriteRenderer::SetupMaterialPropertyBlock(MaterialPropertyBlock& block, const Texture2D* spriteTexture)
{
	const TextureID id = spriteTexture ? spriteTexture->GetTextureID() : TextureID(0);
	const Vector4f texelSize = spriteTexture ? Vector4f(spriteTexture->GetTexelSizeX(), spriteTexture->GetTexelSizeY(), spriteTexture->GetGLWidth(), spriteTexture->GetGLHeight()) : Vector4f(0, 0, 0, 0);

	block.ReplacePropertyTexture(kSLPropMainTex, kTexDim2D, id);
	block.ReplacePropertyVector(kSLPropMainTex_TexelSize, texelSize);
}

const SpriteRenderData* SpriteRenderer::GetSpriteRenderDataInContext(const PPtr<Sprite>& frame)
{
	//@Note: this is here for a possible contextual atlas implementation.
	return &frame->GetRenderDataForPlayMode();
}

void SpriteRenderer::Render (int materialIndex, const ChannelAssigns& channels)
{
	GfxDevice& device = GetGfxDevice();

	Assert(materialIndex == 0);
	if (m_Sprite.IsNull())
		return;
	
	const SpriteRenderData* rd = GetSpriteRenderDataInContext(m_Sprite);
	Assert(rd->texture.IsValid());
	
	PROFILER_AUTO_GFX(gSpriteRenderSingleProfile, this);

	// Get VBO chunk for a rectangle or mesh
	UInt32 numIndices, numVertices;
	GetGeometrySize(numIndices, numVertices);
	if (!numIndices)
		return;
	
	const UInt32 channelMask = (1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor);

	DynamicVBO& vbo = device.GetDynamicVBO();
	UInt8*  __restrict vbPtr;
	UInt16* __restrict ibPtr;
	if ( !vbo.GetChunk(channelMask, numVertices, numIndices, DynamicVBO::kDrawIndexedTriangles, (void**)&vbPtr, (void**)&ibPtr) )
		return;

	TransformSprite (vbPtr, ibPtr, NULL, rd, GetDeviceColor (m_Color, device), 0);
	vbo.ReleaseChunk(numVertices, numIndices);

	// Draw
	if (m_CustomProperties)
		device.SetMaterialProperties(*m_CustomProperties);
	
	PROFILER_BEGIN(gSpriteRenderSubmitVBO, this)
	vbo.DrawChunk(channels);
	GPU_TIMESTAMP();
	PROFILER_END
}

void SpriteRenderer::GetGeometrySize(UInt32& indexCount, UInt32& vertexCount)
{
	if (m_Sprite.IsValid())
	{
		const SpriteRenderData* rd = GetSpriteRenderDataInContext(m_Sprite);
		if (rd->indices.size() > 0)
		{
			indexCount = rd->indices.size();
			vertexCount = rd->vertices.size();
			return;
		}
	}

	indexCount = 0;
	vertexCount = 0;
}

#if GFX_ENABLE_DRAW_CALL_BATCHING
void SpriteRenderer::RenderBatch (const BatchInstanceData* instances, size_t count, size_t numIndices, size_t numVertices, const ChannelAssigns& channels)
{
	DebugAssert(numIndices);
	DebugAssert(numVertices);
	PROFILER_AUTO_GFX(gSpriteRenderBatchProfile, 0);
	
	GfxDevice& device = GetGfxDevice();
	const MaterialPropertyBlock* customProps = count > 0 ? instances[0].renderer->GetCustomProperties() : NULL;
	if (customProps)
		device.SetMaterialProperties (*customProps);
	
	UInt32 expectedFence = device.GetNextCPUFence();
	const UInt32 channelMask = (1<<kShaderChannelVertex) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor);;
	device.BeginDynamicBatching(channels, channelMask, numVertices, numIndices, kPrimitiveTriangles);
	
	for (BatchInstanceData const* it = instances; it < instances + count; ++it)
	{
		UInt32 numIndices, numVertices;
		
		Assert(it->renderer);
		Assert(it->renderer->GetRendererType() == kRendererSprite);
		SpriteRenderer* renderer = (SpriteRenderer*)it->renderer;
		renderer->GetGeometrySize(numIndices, numVertices);
		if (!numIndices)
			continue;
		
		const SpriteRenderData *rd = renderer->GetSpriteRenderDataInContext(renderer->m_Sprite);
		Assert(rd->texture.IsValid());
		
#if ENABLE_MULTITHREADED_CODE
		renderer->m_Sprite->SetCurrentCPUFence(expectedFence);
#endif
		device.DynamicBatchSprite(&it->xform, rd, GetDeviceColor(renderer->m_Color, device));
	}
	device.SetInverseScale(1.0f);
	device.EndDynamicBatching(TransformType(kNoScaleTransform));
	
	// Insert fence after batching is complete
	UInt32 fence  = device.InsertCPUFence();
	Assert(fence == expectedFence);
	GPU_TIMESTAMP();
}

void SpriteRenderer::RenderMultiple (const BatchInstanceData* instances, size_t count, const ChannelAssigns& channels)
{
	size_t numIndicesBatch = 0;
	size_t numVerticesBatch = 0;

	BatchInstanceData const* instancesEnd = instances + count;
	BatchInstanceData const* iBatchBegin = instances;
	BatchInstanceData const* iBatchEnd = instances;
	while (iBatchEnd != instancesEnd)
	{
		Assert(iBatchEnd->renderer->GetRendererType() == kRendererSprite);
		SpriteRenderer* renderer = (SpriteRenderer*)iBatchEnd->renderer;

		if (renderer->GetSprite().IsNull())
		{
			iBatchEnd++;
			continue;
		}
				
		UInt32 numIndices, numVertices;
		renderer->GetGeometrySize(numIndices, numVertices);

		if ((numIndicesBatch + numIndices) <= kMaxNumSpriteTrianglesPerBatch)
		{
			numIndicesBatch += numIndices;
			numVerticesBatch += numVertices;
			iBatchEnd++;
		}
		else
		{
			if (numIndicesBatch)
			{
				RenderBatch(iBatchBegin, iBatchEnd - iBatchBegin, numIndicesBatch, numVerticesBatch, channels);
				numIndicesBatch = 0;
				numVerticesBatch = 0;
				iBatchBegin = iBatchEnd;
			}
			else // Can't fit in one draw call
			{
				RenderBatch(iBatchEnd, 1, numIndices, numVertices, channels);
				iBatchEnd++;
				iBatchBegin = iBatchEnd;
			}
		}
	}

	if ((iBatchBegin != iBatchEnd) && numIndicesBatch)
	{
		RenderBatch(iBatchBegin, iBatchEnd - iBatchBegin, numIndicesBatch, numVerticesBatch, channels);
	}
}
#endif

Material* SpriteRenderer::GetDefaultSpriteMaterial ()
{
	Assert(gSpriteDefaultMaterial);
	return gSpriteDefaultMaterial;
}

#endif // ENABLE_SPRITES
