#include "UnityPrefix.h"
#include "ClothRenderer.h"

#if ENABLE_CLOTH

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Dynamics/Cloth.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"

ClothRenderer::ClothRenderer (MemLabelId label, ObjectCreationMode mode)
:	Super(kRendererCloth, label, mode)
,	m_VBO(NULL)
{
	m_ChannelsInVBO = 0;
	m_UnavailableInVBO = 0;
	m_PauseWhenNotVisible = true;
	m_IsPaused = false;
	m_AABBDirty = true;
	m_AABB.SetCenterAndExtent(Vector3f::zero, Vector3f::zero);
}

ClothRenderer::~ClothRenderer ()
{
	if (m_VBO)
		GetGfxDevice().DeleteVBO(m_VBO);
}

void ClothRenderer::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	ReloadVBOToGfxDevice();
}

void ClothRenderer::Reset ()
{
	Super::Reset ();

	m_PauseWhenNotVisible = true;
}

void ClothRenderer::UpdateClothVBOImmediate (int requiredChannels, UInt32& unavailableChannels)
{
	Unity::Cloth& cloth = GetComponent(InteractiveCloth);

	int supportedChannels = VERTEX_FORMAT3(Vertex, Normal, Color);
	if (cloth.m_UVs.size() == cloth.m_VerticesForRendering->size())
		supportedChannels |= VERTEX_FORMAT1(TexCoord0);
	if (cloth.m_UV1s.size() == cloth.m_VerticesForRendering->size())
		supportedChannels |= VERTEX_FORMAT1(TexCoord1);
	if (cloth.m_Tangents.size() == cloth.m_VerticesForRendering->size())
		supportedChannels |= VERTEX_FORMAT1(Tangent);

	// Silently create an all-white color array if shader wants colors, but mesh does not have them.
	// On D3D, some runtime/driver combinations will crash if a vertex shader wants colors but does not
	// have them (e.g. Vista drivers for Intel 965). In other cases it will default to white for fixed function
	// pipe, and to undefined value for vertex shaders, which is not good either.

	if (cloth.m_Colors.size() != cloth.m_VerticesForRendering->size())
		cloth.m_Colors.resize_initialized( cloth.m_VerticesForRendering->size(), 0xFFFFFFFF );

	int activeChannels = requiredChannels & supportedChannels;
	unavailableChannels = requiredChannels & ~supportedChannels;

	// Set up vertex buffer channels and streams
	VertexBufferData vertexBuffer;
	UInt32 stride = 0;
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		ChannelInfo& info = vertexBuffer.channels[i];
		if (activeChannels & (1 << i))
		{
			info.stream = 0;
			info.offset = stride;
            info.format = VBO::GetDefaultChannelFormat(i);
            info.dimension = VBO::GetDefaultChannelDimension(i);
			stride += VBO::GetDefaultChannelByteSize(i);
		}
		else
  			info.Reset();
  	}
	vertexBuffer.streams[0].channelMask = activeChannels;
	vertexBuffer.streams[0].stride = stride;

	// Don't pass a buffer since we use map/unmap for writing
	vertexBuffer.buffer = NULL;
	int vertexCount = cloth.m_NumVerticesForRendering;
	vertexBuffer.bufferSize = stride * vertexCount;
	vertexBuffer.vertexCount = vertexCount;
	m_VBO->SetVertexStreamMode(0, VBO::kStreamModeWritePersist);
	m_VBO->UpdateVertexData(vertexBuffer);

	IndexBufferData indexBuffer;
	indexBuffer.indices = &(*cloth.m_IndicesForRendering)[0];
	indexBuffer.count = cloth.m_NumIndicesForRendering;
	indexBuffer.hasTopologies = (1<<kPrimitiveTriangles);

	VertexStreamData mappedStream;
	if (!m_VBO->MapVertexStream(mappedStream, 0))
		return;
	UInt8* buffer = mappedStream.buffer;
	for (int v = 0; v < vertexCount; v++)
	{
		if (activeChannels & (1 << kShaderChannelVertex))
		{
			*(Vector3f*)buffer = (*cloth.m_VerticesForRendering)[v];
			buffer += sizeof(Vector3f);
		}

		if (activeChannels & (1 << kShaderChannelNormal))
		{
			*(Vector3f*)buffer = (*cloth.m_NormalsForRendering)[v];
			buffer += sizeof(Vector3f);
		}

		if (activeChannels & (1 << kShaderChannelColor))
		{
			*(ColorRGBA32*)buffer = cloth.m_Colors[v];
			buffer += sizeof(ColorRGBA32);
		}

		if (activeChannels & (1 << kShaderChannelTexCoord0))
		{
			*(Vector2f*)buffer = cloth.m_UVs[v];
			buffer += sizeof(Vector2f);
		}

		if (activeChannels & (1 << kShaderChannelTexCoord1))
		{
			*(Vector2f*)buffer = cloth.m_UV1s[v];
			buffer += sizeof(Vector2f);
		}

		if (activeChannels & (1 << kShaderChannelTangent))
		{
			*(Vector4f*)buffer = cloth.m_Tangents[v];
			buffer += sizeof(Vector4f);
		}
	}
	m_VBO->UnmapVertexStream(0);

	m_ChannelsInVBO = activeChannels;
	m_VBO->UpdateIndexData (indexBuffer);
}

void ClothRenderer::UpdateClothVerticesFromPhysics()
{
	Unity::Cloth& cloth = GetComponent(InteractiveCloth);
	cloth.ProcessMeshForRenderer();
	cloth.m_NumVerticesFromPhysX = 0;

	UpdateAABB();
}

PROFILER_INFORMATION(gClothRenderProfile, "DeformableMeshRenderer.Render", kProfilerRender)

void ClothRenderer::Render (int/* subsetIndex*/, const ChannelAssigns& channels)
{
	PROFILER_AUTO(gClothRenderProfile, this)

	UInt32 requiredChannels = channels.GetSourceMap();

	Unity::Cloth& cloth = GetComponent(InteractiveCloth);
	if (cloth.m_NumVertices > 0 || cloth.m_NumVerticesFromPhysX > 0)
	{
		if ((requiredChannels | m_ChannelsInVBO) != m_ChannelsInVBO || cloth.m_NumVerticesFromPhysX || m_VBO->IsVertexBufferLost())
		{
			if (cloth.m_NumVerticesFromPhysX)
				UpdateClothVerticesFromPhysics();

			UpdateClothVBOImmediate(requiredChannels, m_UnavailableInVBO);
		}

		if (m_CustomProperties)
			GetGfxDevice().SetMaterialProperties (*m_CustomProperties);
		m_VBO->DrawVBO (channels, 0, cloth.m_NumIndices, kPrimitiveTriangles, 0, cloth.m_NumVerticesForRendering);
	}
	GPU_TIMESTAMP();
}

void ClothRenderer::UpdateAABB()
{
	Unity::Cloth& cloth = GetComponent(InteractiveCloth);
	dynamic_array<Vector3f> &vertices = *cloth.m_VerticesForRendering;
	if (cloth.m_NumVertices != 0)
	{
		m_AABB.SetCenterAndExtent(vertices[0], Vector3f::zero);
		for (int i=0; i<cloth.m_NumVertices; i++)
			m_AABB.Encapsulate(vertices[i]);
	}
	else
		m_AABB.SetCenterAndExtent(Vector3f::zero, Vector3f::zero);

	BoundsChanged();
	m_AABBDirty = false;
}

void ClothRenderer::RendererBecameVisible ()
{
	Super::RendererBecameVisible();
	Unity::Cloth& cloth = GetComponent(InteractiveCloth);
	if (m_IsPaused)
	{
		m_IsPaused = false;
		cloth.SetSuspended(false);
	}
}

void ClothRenderer::RendererBecameInvisible ()
{
	Super::RendererBecameInvisible();
	Unity::Cloth& cloth = GetComponent(InteractiveCloth);
	if (m_PauseWhenNotVisible)
	{
		m_IsPaused = true;
		cloth.SetSuspended(true);
	}
}

void ClothRenderer::UpdateTransformInfo ()
{
	{ // transform
		m_TransformInfo.worldMatrix.SetIdentity ();
		m_TransformInfo.transformType = kNoScaleTransform;
		m_TransformInfo.invScale = 1.0F;
	}
	if (m_AABBDirty)
		UpdateClothVerticesFromPhysics ();

	m_TransformInfo.worldAABB = m_AABB;

	Transform& t = GetComponent(Transform);
	TransformAABB( m_AABB, t.GetWorldToLocalMatrixNoScale(), m_TransformInfo.localAABB );
}


void ClothRenderer::UnloadVBOFromGfxDevice()
{
	if (m_VBO)
	{
		GetGfxDevice().DeleteVBO (m_VBO);
	}
	m_VBO = NULL;
}


void ClothRenderer::ReloadVBOToGfxDevice()
{
	m_VBO = GetGfxDevice().CreateVBO();
	m_VBO->SetVertexStreamMode(0, VBO::kStreamModeDynamic);
	m_VBO->SetIndicesDynamic(true);
	m_ChannelsInVBO = 0;
	m_UnavailableInVBO = 0;
}


template<class TransferFunction>
void ClothRenderer::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	TRANSFER (m_PauseWhenNotVisible);
}

IMPLEMENT_CLASS (ClothRenderer)
IMPLEMENT_OBJECT_SERIALIZE (ClothRenderer)

#endif // ENABLE_CLOTH
