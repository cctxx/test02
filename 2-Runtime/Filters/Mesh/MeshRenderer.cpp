#include "UnityPrefix.h"
#include "MeshRenderer.h"
#include "Runtime/Graphics/Transform.h"
#include "LodMesh.h"
#include "Runtime/Filters/Mesh/MeshUtility.h"
#include "Runtime/Graphics/DrawUtil.h"
#include "Runtime/GfxDevice/BatchRendering.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/GfxDevice/GfxDevice.h"

#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/shaderlab.h"

#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Camera/RenderLoops/BuiltinShaderParamUtility.h"
#include "Runtime/GfxDevice/BatchRendering.h"

#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/GfxDevice/GfxDeviceStats.h"
#include "Runtime/Misc/BuildSettings.h"


PROFILER_INFORMATION(gMeshRenderProfile, "MeshRenderer.Render", kProfilerRender)
PROFILER_INFORMATION(gMeshRenderScaledProfile, "MeshRenderer.ComputeScaledMesh", kProfilerRender)
PROFILER_INFORMATION(gMeshRenderStaticBatch, "MeshRenderer.RenderStaticBatch", kProfilerRender)
PROFILER_INFORMATION(gMeshRenderDynamicBatch, "MeshRenderer.RenderDynamicBatch", kProfilerRender)


#if UNITY_EDITOR
#define SET_CACHED_SURFACE_AREA_DIRTY() m_CachedSurfaceArea = -1.0f;
#else
#define SET_CACHED_SURFACE_AREA_DIRTY() //do nothing
#endif

IMPLEMENT_CLASS_INIT_ONLY (MeshRenderer)

MeshRenderer::MeshRenderer (MemLabelId label, ObjectCreationMode mode)
:	Super(kRendererMesh, label, mode)
,	m_MeshNode (this)
{
	m_ScaledMeshDirty = true;
	m_MeshWasModified = false;

	m_CachedMesh = NULL;
	m_ScaledMesh = NULL;
	SET_CACHED_SURFACE_AREA_DIRTY();
}

MeshRenderer::~MeshRenderer ()
{
	FreeScaledMesh ();
}

void MeshRenderer::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	UpdateCachedMesh ();
}

void MeshRenderer::Deactivate (DeactivateOperation operation)
{
	Super::Deactivate (operation);
	FreeScaledMesh ();
}

void MeshRenderer::InitializeClass ()
{
	REGISTER_MESSAGE (MeshRenderer, kTransformChanged, TransformChanged, int);

	REGISTER_MESSAGE_VOID(MeshRenderer, kDidModifyBounds, DidModifyMeshBounds);
	REGISTER_MESSAGE_VOID(MeshRenderer, kDidDeleteMesh, DidDeleteMesh);
	REGISTER_MESSAGE_VOID(MeshRenderer, kDidModifyMesh, DidModifyMesh);
}

void MeshRenderer::TransformChanged (int changeMask)
{
	if (changeMask & Transform::kScaleChanged)
	{
		SET_CACHED_SURFACE_AREA_DIRTY();
		m_ScaledMeshDirty = true;
	}
	Super::TransformChanged (changeMask);
}

void MeshRenderer::UpdateLocalAABB()
{
	DebugAssertIf( m_CachedMesh != m_Mesh );
	if( m_CachedMesh )
	{
		if (HasSubsetIndices())
		{
			if (GetMaterialCount() == 1)
				m_TransformInfo.localAABB = m_CachedMesh->GetBounds(GetSubsetIndex(0));
			else
			{
				MinMaxAABB minMaxAABB;
				for (int m = 0; m < GetMaterialCount(); ++m)
					minMaxAABB.Encapsulate(m_CachedMesh->GetBounds(GetSubsetIndex(m)));
				m_TransformInfo.localAABB = minMaxAABB;
			}
		}
		else
		{
			m_TransformInfo.localAABB = m_CachedMesh->GetBounds();
		}
	}
	else
		m_TransformInfo.localAABB.SetCenterAndExtent( Vector3f::zero, Vector3f::zero );
}

void MeshRenderer::SetSubsetIndex(int subsetIndex, int index)
{
	Renderer::SetSubsetIndex(subsetIndex, index);

	// Reset scaled mesh if this renderer is now statically batched.
	// Mesh scaling should never be used with static batching (case 551504).
	FreeScaledMesh();
}

int MeshRenderer::GetStaticBatchIndex() const
{
	// Wrap non-virtual version in a virtual call
	return GetMeshStaticBatchIndex();
}

int MeshRenderer::GetMeshStaticBatchIndex() const
{
	return IsPartOfStaticBatch() ? m_CachedMesh->GetInstanceID(): 0;
}

UInt32 MeshRenderer::GetMeshIDSmall() const
{
	return m_CachedMesh ? m_CachedMesh->GetInternalMeshID(): 0;
}


Mesh* MeshRenderer::GetCachedMesh ()
{
	DebugAssertIf(m_CachedMesh != m_Mesh);
	return m_CachedMesh;
}


Mesh* MeshRenderer::GetMeshUsedForRendering ()
{
	Mesh* cachedMesh = GetCachedMesh ();

	if (cachedMesh != NULL)
	{
		// NOTE: staticaly batched geometry already has scale applied
		// therefore we skip mesh scaling
		if (!m_ScaledMeshDirty || IsPartOfStaticBatch())
			return m_ScaledMesh == NULL ? cachedMesh : m_ScaledMesh->mesh;

		m_ScaledMeshDirty = false;

		float unused2;
		Matrix4x4f unused;
		Matrix4x4f scalematrix;
		TransformType type = GetTransform().CalculateTransformMatrixDisableNonUniformScale (unused, scalematrix, unused2);
		// Check if no scale is needed or we can't access vertices anyway to transform them correctly
		DebugAssert(!IsNonUniformScaleTransform(type) || cachedMesh->HasVertexData());
		if (!IsNonUniformScaleTransform(type) || !cachedMesh->HasVertexData())
		{
			// Cleanup scaled mesh
			FreeScaledMesh();
			m_MeshWasModified = false;

			return cachedMesh;
		}
		// Need scaled mesh
		else
		{
			// Early out if the mesh scale hasn't actually changed
			if (m_ScaledMesh != NULL && CompareApproximately(scalematrix, m_ScaledMesh->matrix) && !m_MeshWasModified)
				return m_ScaledMesh->mesh;

			// Scale has changed, maybe generated a new scaled mesh
			PROFILER_AUTO(gMeshRenderScaledProfile, this)

			// Allocate scaled mesh
			if (m_ScaledMesh == NULL)
			{
				m_ScaledMesh = new ScaledMesh ();
				m_ScaledMesh->mesh = NEW_OBJECT (Mesh);
				m_ScaledMesh->mesh->Reset();
				m_ScaledMesh->mesh->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
				m_ScaledMesh->mesh->SetHideFlags(kHideAndDontSave);
			}

			m_MeshWasModified = false;

			// Rescale mesh
			m_ScaledMesh->matrix = scalematrix;
			m_ScaledMesh->mesh->CopyTransformed(*cachedMesh, scalematrix);
			return m_ScaledMesh->mesh;
		}
	}
	else
	{
		return NULL;
	}
}

static SubMesh const& GetSubMesh(Mesh& mesh, int subsetIndex)
{
	const int subMeshCount = mesh.GetSubMeshCount()? mesh.GetSubMeshCount()-1 : 0;
	const int subMeshIndex = std::min<unsigned int>(subsetIndex, subMeshCount);
	return mesh.GetSubMeshFast(subMeshIndex);
}


void MeshRenderer::Render (int subsetIndex, const ChannelAssigns& channels)
{
	PROFILER_AUTO(gMeshRenderProfile, this);

	Mesh* mesh = GetMeshUsedForRendering ();
	if (!mesh)
		return;
	if (m_CustomProperties)
		GetGfxDevice().SetMaterialProperties (*m_CustomProperties);
	DrawUtil::DrawMeshRaw (channels, *mesh, subsetIndex);
}


#if UNITY_EDITOR

void MeshRenderer::GetRenderStats (RenderStats& renderStats)
{
	///@TODO: This does not work with static batching fixor it.
	memset(&renderStats, 0, sizeof(renderStats));

	Mesh* mesh = m_Mesh;
	if (mesh)
	{
		for (int i=0;i<GetMaterialCount();i++)
		{
			const SubMesh& submesh = GetSubMesh (*mesh, GetSubsetIndex(i));

			renderStats.triangleCount += GetPrimitiveCount(submesh.indexCount, submesh.topology, false);
			renderStats.vertexCount += submesh.vertexCount;
			renderStats.submeshCount++;
		}
	}
}

float MeshRenderer::GetCachedSurfaceArea ()
{
	if (m_CachedSurfaceArea >= 0.0f)
		return m_CachedSurfaceArea;

	Mesh* mesh = GetCachedMesh ();
	if (!mesh)
	{
		m_CachedSurfaceArea = 1.0f;
		return m_CachedSurfaceArea;
	}

	Matrix4x4f objectToWorld;
	GetComponent (Transform).CalculateTransformMatrix (objectToWorld);

	Mesh::TemporaryIndexContainer triangles;
	mesh->GetTriangles (triangles);

	dynamic_array<Vector3f> vertices (mesh->GetVertexCount(), kMemTempAlloc);
	mesh->ExtractVertexArray (vertices.begin ());

	m_CachedSurfaceArea = CalculateSurfaceArea (objectToWorld, triangles, vertices);

	return m_CachedSurfaceArea;
}
#endif

void MeshRenderer::DidModifyMeshBounds ()
{
	SET_CACHED_SURFACE_AREA_DIRTY();
	m_TransformDirty = true;
	BoundsChanged ();
}

void MeshRenderer::DidModifyMesh ()
{
	m_MeshWasModified = true;
	m_ScaledMeshDirty = true;
	m_TransformDirty = true;
	BoundsChanged();
}

void MeshRenderer::DidDeleteMesh ()
{
	m_CachedMesh = NULL;
}

void MeshRenderer::SetSharedMesh (PPtr<Mesh> mesh)
{
	SET_CACHED_SURFACE_AREA_DIRTY();
	m_Mesh = mesh;
	UpdateCachedMesh ();
}

PPtr<Mesh> MeshRenderer::GetSharedMesh ()
{
	return m_Mesh;
}

void MeshRenderer::UpdateCachedMesh ()
{
	Mesh* mesh = m_Mesh;
	if (mesh != m_CachedMesh)
	{
		// In order to make sure we are not using old subset indices referring to the previous mesh
		// we clear them here, assuming that the correct subset indices will be set subsequently.
		// We only do this if there was a previous mesh that the new mesh is replacing, since some
		// code paths are transferring in the values and then call this function. In that case we do
		// not want to mess with the indices.
		if (m_CachedMesh) ClearSubsetIndices();
		m_ScaledMeshDirty = true;
		m_MeshWasModified = true;
		m_CachedMesh = mesh;
		m_TransformDirty = true;
		BoundsChanged();
		m_MeshNode.RemoveFromList();
		if (m_CachedMesh)
			m_CachedMesh->AddObjectUser( m_MeshNode );
	}
}

void MeshRenderer::FreeScaledMesh ()
{
	if (m_ScaledMesh)
	{
		DestroySingleObject (m_ScaledMesh->mesh);
		delete m_ScaledMesh;
		m_ScaledMesh = NULL;
		m_ScaledMeshDirty = false;
	}
}

#if GFX_ENABLE_DRAW_CALL_BATCHING

PROFILER_INFORMATION(gDrawStaticBatchProfile, "Batch.DrawStatic", kProfilerRender)
PROFILER_INFORMATION(gDrawDynamicBatchProfile, "Batch.DrawDynamic", kProfilerRender)

static bool RenderStaticBatch (Mesh& mesh, VBO& vbo,
							   BatchInstanceData const* instances, size_t count, const ChannelAssigns& channels)
{
	if (count <= 1)
		return false;
	IndexBufferData indexBuffer;
	mesh.GetIndexBufferData (indexBuffer);
	if (!indexBuffer.indices)
		return false;

	PROFILER_AUTO(gMeshRenderStaticBatch, &mesh)

	const SubMesh& firstSubmesh = GetSubMesh (mesh, instances[0].subsetIndex);
	GfxPrimitiveType topology = firstSubmesh.topology;
	const Matrix4x4f& xform = instances[0].xform;
	int xformType = instances[0].xformType;

	GfxDevice& device = GetGfxDevice();
	device.BeginStaticBatching(channels, topology);

	// Concat SubMeshes
	for (BatchInstanceData const* it = instances; it < instances + count; ++it)
	{
		const SubMesh& submesh = GetSubMesh (mesh, it->subsetIndex);
		device.StaticBatchMesh(submesh.firstVertex, submesh.vertexCount, indexBuffer, submesh.firstByte, submesh.indexCount);

		Assert(topology == submesh.topology);
		Assert(xformType == it->xformType);
	}

	device.EndStaticBatching(vbo, xform, TransformType(xformType), mesh.GetChannelsInVBO());
	GPU_TIMESTAMP();

#if ENABLE_MULTITHREADED_CODE
	// Make sure renderer is done before mesh is changed or deleted
	UInt32 cpuFence = device.InsertCPUFence();
	mesh.SetCurrentCPUFence(cpuFence);
#endif

	return true;
}

static bool RenderDynamicBatch (BatchInstanceData const* instances, size_t count, size_t maxVertices, size_t maxIndices, const ChannelAssigns& shaderChannels, UInt32 availableChannels, GfxPrimitiveType topology)
{
	if (count <= 1)
		return false;

	if (gGraphicsCaps.buggyDynamicVBOWithTangents && (shaderChannels.GetSourceMap() & (1<<kShaderChannelTangent)))
		return false;

	PROFILER_AUTO(gMeshRenderDynamicBatch, NULL)

	DebugAssert (topology != -1);

	GfxDevice& device = GetGfxDevice();
	UInt32 expectedFence = device.GetNextCPUFence();
	device.BeginDynamicBatching(shaderChannels, availableChannels, maxVertices, maxIndices, topology);

	// Transform on CPU
	int xformType = -1;


	for (BatchInstanceData const* it = instances; it < instances + count; ++it)
	{
		Assert(it->renderer);
		Assert(it->renderer->GetRendererType() == kRendererMesh);
		MeshRenderer* meshRenderer = (MeshRenderer*)it->renderer;
		Mesh* mesh = meshRenderer->GetMeshUsedForRendering();
		if (!mesh)
			continue;

		SubMesh const& submesh = GetSubMesh (*mesh, it->subsetIndex);

		Assert(topology == ~0UL || topology == submesh.topology);
		Assert(xformType == -1 || xformType == it->xformType);
		xformType = it->xformType;

		VertexBufferData vbData;
		mesh->GetVertexBufferData(vbData, availableChannels);
		IndexBufferData ibData;
		mesh->GetIndexBufferData(ibData);

		// Make sure renderer is done before mesh is changed or deleted
#if ENABLE_MULTITHREADED_CODE
		mesh->SetCurrentCPUFence(expectedFence);
#endif

		device.DynamicBatchMesh(it->xform, vbData, submesh.firstVertex, submesh.vertexCount, ibData, submesh.firstByte, submesh.indexCount);
	}

	// Draw
	Assert(xformType != -1);
	Assert(topology != ~0UL);

	// We transformed all geometry into the world (Identity) space already.
	// However, we did not normalize the normals.
	// In fixed function, most GfxDevices (e.g. OpenGL & D3D) will try to figure out uniform
	// scale directly from the matrix, and hence will not scale our normals.
	// Therefore we upgrade normalization mode to "full normalize" to make them transform properly.
	if (xformType & kUniformScaleTransform)
	{
		xformType &= ~kUniformScaleTransform;
		xformType |= kNonUniformScaleTransform;
	}

	// Caveat: we do pass identity matrix when batching
	// currently normals handling in vprog is:
	// xform * (normalize(normal) * unity_Scale.w);
	// as we pass identity matrix (no scale) we need NOT apply inv_scale
	device.SetInverseScale(1.0f);
	device.EndDynamicBatching(TransformType(xformType));

	// Insert fence after batching is complete
	UInt32 fence = device.InsertCPUFence();
	Assert(fence == expectedFence);

	GPU_TIMESTAMP();
	
	return true;
}

void MeshRenderer::RenderMultiple (BatchInstanceData const* instances, size_t count, const ChannelAssigns& channels)
{
	Assert(count > 0);

	GfxDevice& device = GetGfxDevice();
	const float invScale = device.GetBuiltinParamValues().GetInstanceVectorParam(kShaderInstanceVecScale).w;

	const MaterialPropertyBlock* customProps = instances[0].renderer->GetCustomProperties();
	if (customProps)
		device.SetMaterialProperties (*customProps);

	const UInt32 wantedChannels = channels.GetSourceMap();
	const bool enableDynamicBatching = GetBuildSettings().enableDynamicBatching;

	BatchInstanceData const* instancesEnd = instances + count;
	for (BatchInstanceData const* iBatchBegin = instances; iBatchBegin != instancesEnd; )
	{
		Assert(iBatchBegin->renderer->GetRendererType() == kRendererMesh);
		MeshRenderer* meshRenderer = (MeshRenderer*)iBatchBegin->renderer;
		Mesh* mesh = meshRenderer->GetMeshUsedForRendering ();
		VBO* vbo = mesh ? mesh->GetSharedVBO (wantedChannels) : NULL;
		if (!vbo)
		{
			// Skip mesh
			++iBatchBegin;
			continue;
		}

		const UInt32 availableChannels = mesh->GetChannelsInVBO() & wantedChannels;
		const int staticBatchIndex = meshRenderer->GetMeshStaticBatchIndex ();
		const int xformType = iBatchBegin->xformType;

		const SubMesh& firstSubMesh = GetSubMesh(*mesh, iBatchBegin->subsetIndex);
		const GfxPrimitiveType topology = firstSubMesh.topology;
		size_t batchVertexCount = firstSubMesh.vertexCount;
		size_t batchIndexCount = firstSubMesh.indexCount;

		// For first strip take 1 connecting (degenerate) triangles into account
		if (topology == kPrimitiveTriangleStripDeprecated)
			batchIndexCount += 1;

		BatchInstanceData const* iBatchEnd = iBatchBegin + 1;

		// static batching
		if (staticBatchIndex != 0)
		{
			Assert(topology == kPrimitiveTriangles || topology == kPrimitiveTriangleStripDeprecated);
			const int maxIndices = GetGfxDevice().GetMaxStaticBatchIndices();

			for (; iBatchEnd != instancesEnd; ++iBatchEnd)
			{
				if (xformType != iBatchEnd->xformType)
					break;

				Assert(iBatchEnd->renderer->GetRendererType() == kRendererMesh);
				MeshRenderer* meshRenderer = (MeshRenderer*)iBatchEnd->renderer;
				if (staticBatchIndex != meshRenderer->GetMeshStaticBatchIndex())
					break;

				Mesh* nextMesh = meshRenderer->GetMeshUsedForRendering ();
				if (!nextMesh)
					break;

				const SubMesh& submesh = GetSubMesh(*nextMesh, iBatchEnd->subsetIndex);
				if (submesh.topology != topology)
					break;

				VBO* nextVbo = nextMesh->GetSharedVBO (wantedChannels);
				if (nextVbo != vbo) // also a NULL check since vbo is non-NULL
					break;

				UInt32 nextAvailableChannels = nextMesh->GetChannelsInVBO() & wantedChannels;
				if (availableChannels != nextAvailableChannels)
					break;

				UInt32 requiredIndexCount = batchIndexCount + submesh.indexCount;
				if (topology == kPrimitiveTriangleStripDeprecated)
					requiredIndexCount += 3; // take 3 connecting (degenerate) triangles into account

				if (requiredIndexCount > maxIndices)
					break;

				batchIndexCount = requiredIndexCount;
			}

			if (mesh && vbo)
				if (RenderStaticBatch (*mesh, *vbo, iBatchBegin, iBatchEnd - iBatchBegin, channels))
					iBatchBegin = iBatchEnd;
		}
		else if (vbo && enableDynamicBatching)
		// dynamic batching
		{
			const int firstVertexCount = batchVertexCount;
			const int firstIndexCount  = batchIndexCount;

			// after moving to fully strided meshes we were hit by the issue that we might have different channels
			// in src and dst data, so our optimized asm routines doesn't quite work.
			// we will move to support vertex streams (this will solve lots of issues after skinning/batching asm rewrite ;-))
			// but for now let just play safe

			if (CanUseDynamicBatching(*mesh, wantedChannels, firstVertexCount) &&
				firstIndexCount < kDynamicBatchingIndicesThreshold &&
				topology != kPrimitiveLineStrip)
			{
				for (; iBatchEnd != instancesEnd; ++iBatchEnd)
				{
					if (xformType != iBatchEnd->xformType)
						break;

					Assert(iBatchEnd->renderer->GetRendererType() == kRendererMesh);
					MeshRenderer* meshRenderer = (MeshRenderer*)iBatchEnd->renderer;
					if (meshRenderer->IsPartOfStaticBatch())
						break;

					Mesh* nextMesh = meshRenderer->GetMeshUsedForRendering ();
					if (!nextMesh)
						break;

					const SubMesh& submesh = GetSubMesh(*nextMesh, iBatchEnd->subsetIndex);
					if (submesh.topology != topology)
						break;

					if (!CanUseDynamicBatching(*nextMesh, wantedChannels, submesh.vertexCount))
						break;

					UInt32 requiredVertexCount = batchVertexCount + submesh.vertexCount;
					UInt32 requiredIndexCount = batchIndexCount + submesh.indexCount;
					if (topology == kPrimitiveTriangleStripDeprecated)
						requiredIndexCount += 3; // take 3 connecting (degenerate) triangles into account

					if (requiredVertexCount > 0xffff)
						break;

					if (requiredIndexCount > kDynamicBatchingIndicesThreshold)
						break;

					VBO* nextVbo = nextMesh->GetSharedVBO (wantedChannels);
					if (!nextVbo)
						break;

					const UInt32 nextAvailableChannels = nextMesh->GetChannelsInVBO() & wantedChannels;
					if (availableChannels != nextAvailableChannels)
						break;

					batchVertexCount = requiredVertexCount;
					batchIndexCount = requiredIndexCount;
				}

				// Skip batch if batchVertexCount == 0 or batchIndexCount == 0
				if (batchVertexCount == 0 || batchIndexCount == 0 || RenderDynamicBatch (iBatchBegin, iBatchEnd - iBatchBegin, batchVertexCount, batchIndexCount, channels, availableChannels, topology))
					iBatchBegin = iBatchEnd;
			}
		}

		// old-school rendering for anything left
		for (; iBatchBegin != iBatchEnd; ++iBatchBegin)
		{
			BatchInstanceData const* it = iBatchBegin;
			Assert(iBatchBegin->renderer->GetRendererType() == kRendererMesh);
			MeshRenderer* meshRenderer = (MeshRenderer*)iBatchBegin->renderer;
			Mesh* mesh = meshRenderer->GetMeshUsedForRendering ();
			if (!mesh)
				continue;

			VBO* vbo = mesh->GetSharedVBO (wantedChannels);
			if (!vbo)
				continue;

			if (customProps)
				device.SetMaterialProperties (*customProps);

			// Batched rendering above will have set inverse scale to 1.0 (since everything is transformed
			// to identity). For remaining meshes that aren't batched, we have to setup the original scale
			// back.
			device.SetInverseScale(invScale);
			SetupObjectMatrix (it->xform, it->xformType);
			DrawUtil::DrawVBOMeshRaw (*vbo, *mesh, channels, it->subsetIndex);
		}

		Assert(iBatchBegin == iBatchEnd); // everything was rendered successfully
	}
}

bool MeshRenderer::CanUseDynamicBatching(const Mesh& mesh, UInt32 wantedChannels, int vertexCount)
{
	if (mesh.GetStreamCompression() != Mesh::kStreamCompressionDefault ||
		mesh.GetIndexBuffer().empty() ||
		vertexCount > kDynamicBatchingVerticesThreshold ||
		vertexCount * BitsInMask(wantedChannels) > kDynamicBatchingVertsByChannelThreshold)
		return false;
	return true;
}

#endif // #if GFX_ENABLE_DRAW_CALL_BATCHING

