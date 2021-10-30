#include "UnityPrefix.h"
#include "SkinnedMeshFilter.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Filters/Mesh/MeshSkinning.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Filters/Mesh/MeshBlendShaping.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Filters/Mesh/MeshUtility.h"
#include "Runtime/Graphics/DrawUtil.h"
#include "Runtime/GameCode/CallDelayed.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Threads/JobScheduler.h"
#include "Runtime/Dynamics/SkinnedCloth.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/BaseClasses/CleanupManager.h"
#include "Runtime/BaseClasses/EventIDs.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Interfaces/IPhysics.h"
#include "Runtime/Interfaces/IAnimation.h"
#include "BlendShapeAnimationBinding.h"
#if UNITY_PS3
#include "Runtime/GfxDevice/ps3/GfxGCMVBO.h"
#include "Runtime/Filters/Mesh/MeshPartitioner.h"
#endif

PROFILER_INFORMATION(gMeshSkinningUpdate, "MeshSkinning.Update", kProfilerRender)
PROFILER_INFORMATION(gMeshSkinningUpdateImmediate, "MeshSkinning.UpdateImmediate", kProfilerRender)
PROFILER_INFORMATION(gMeshSkinningPrepare, "MeshSkinning.Prepare", kProfilerRender)
PROFILER_INFORMATION(gMeshSkinningRender, "MeshSkinning.Render", kProfilerRender)
PROFILER_INFORMATION(gMeshSkinningWait, "MeshSkinning.WaitForSkinThreads", kProfilerRender)
PROFILER_INFORMATION(gMeshSkinningSkinGPU, "MeshSkinning.SkinOnGPU", kProfilerRender)


#if UNITY_EDITOR
#define SET_CACHED_SURFACE_AREA_DIRTY() m_CachedSurfaceArea = -1.0f;
#else
#define SET_CACHED_SURFACE_AREA_DIRTY() // do nothing
#endif

typedef List< ListNode<SkinnedMeshRenderer> > SkinnedMeshList;
static SkinnedMeshList gActiveSkinnedMeshes;

/*
JOE:
 * TODO: Do we really need this.  ->   Test cloth

MIRCEA:

 * PS3 version for fallback skinning ( so mesh particle emitters & skinned cloth still works )

*/

SkinnedMeshRenderer::SkinnedMeshRenderer (MemLabelId label, ObjectCreationMode mode)
:	Super(kRendererSkinnedMesh, label, mode)
,	m_BlendShapeWeights(0, label)
,	m_CachedAnimator(NULL)
,	m_CachedBlendShapeCount (0)
,	m_SkinNode(this)
,	m_MeshNode(this)
,	m_AABB(Vector3f::zero, Vector3f::zero)
,	m_MemExportInfo(0)
{
	m_Visible = false;
	m_UpdateBeforeRendering = false;
	m_SourceMeshDirty = false;
	m_DirtyAABB = true;
	m_CachedMesh = NULL;
	m_ChannelsInVBO = 0;
	m_Cloth = NULL;
	m_VBO = NULL;
	SET_CACHED_SURFACE_AREA_DIRTY();
}

SkinnedMeshRenderer::~SkinnedMeshRenderer ()
{
	Assert(m_CachedAnimator == NULL);

	if(m_MemExportInfo)
		GetGfxDevice().DeleteGPUSkinningInfo(m_MemExportInfo);

	if (m_VBO)
	{
		GetGfxDevice().DeleteVBO(m_VBO);
		m_VBO = NULL;
	}
}

void SkinnedMeshRenderer::Setup (Mesh* mesh, const dynamic_array<PPtr<Transform> >& state)
{
	m_Bones = state;
	m_Mesh = mesh;
	UpdateCachedMesh ();
	SetDirty();
}

void SkinnedMeshRenderer::SetMesh (Mesh*  mesh)
{
	m_Mesh = mesh;
	UpdateCachedMesh ();
	SetDirty();
}

Mesh* SkinnedMeshRenderer::GetMesh ()
{
	return m_Mesh;
}

void SkinnedMeshRenderer::SetBones (const dynamic_array<PPtr<Transform> >& bones)
{
	m_Bones = bones;
	SetDirty();
	if (!bones.empty())
		ClearCachedAnimatorBinding(); // switch to non-optimized mode, no binding is needed anymore
}

void SkinnedMeshRenderer::Reset()
{
	Super::Reset();
	m_Quality = 0;
	m_UpdateWhenOffscreen = false;
	m_AABB = AABB(Vector3f::zero, Vector3f::zero);
}


bool SkinnedMeshRenderer::DoesQualifyForMemExport() const
{
	bool qualifies = (m_Cloth == 0) && (m_MemExportInfo);
	qualifies = qualifies && GetPlayerSettings().GetGPUSkinning();

	return qualifies;
}

bool SkinnedMeshRenderer::CalculateBoneBasedBounds (const Matrix4x4f* poseMatrices, size_t size, MinMaxAABB& output)
{
	if (m_CachedMesh == NULL)
		return false;

	if (m_CachedMesh->GetMaxBoneIndex() >= size)
		return false;

	const Mesh::AABBContainer& bounds = m_CachedMesh->GetCachedBonesBounds ();
	if (size > bounds.size())
		return false;
        
	MinMaxAABB minMaxAABB;
	for(int i=0;i<size;i++)
	{
		AABB result;

		///@TODO: OPTIMIZATION: DO this precomputed.
		if (!bounds[i].IsValid())
			continue;

		AABB aabb = bounds[i];

		TransformAABB(aabb, poseMatrices[i], result);
		// TransformAABBSlow (aabb, animatedPose[i], result);

		minMaxAABB.Encapsulate(result.GetMin());
		minMaxAABB.Encapsulate(result.GetMax());
	}

	output = minMaxAABB;
	return true;
}

#if UNITY_EDITOR
bool SkinnedMeshRenderer::CalculateVertexBasedBounds (const Matrix4x4f* poseMatrices, MinMaxAABB& output)
{
	if (m_CachedMesh == NULL)
		return false;

	int boneCount = m_CachedMesh->GetBindposeCount ();

	Matrix4x4f* fullMatrices = NULL;
	ALLOC_TEMP(fullMatrices, Matrix4x4f, boneCount);

	const Matrix4x4f* bindPoses = m_CachedMesh->GetBindposes();
	for (int i=0;i<boneCount;i++)
	{
		MultiplyMatrices4x4(&poseMatrices[i], &bindPoses[i], &fullMatrices[i]);
	}

	MinMaxAABB minMaxAABB;

	// could actually use only the vertices which are affected by bones which are actually animated
	StrideIterator<Vector3f> vertices = m_CachedMesh->GetVertexBegin();
	const BoneInfluence* boneInfluences = m_CachedMesh->GetBoneWeights ();
	int count = m_CachedMesh->GetVertexCount();
	if (count == 0)
		minMaxAABB.Encapsulate(Vector3f::zero);
	for (int i = 0; i < count; ++i, ++vertices)
	{
		const BoneInfluence& boneInfluence = boneInfluences[i];
		Vector3f v(Vector3f::zero);

		float w = 0;

		// This always calculates bounding box as if it was 4 bone skinning
		// 2 bone or 1 bone skinning could produce slightly different results
		for (int j = 0; j < 4; ++j)
		{
			const int boneIndex = boneInfluence.boneIndex[j];
			Assert(boneIndex < boneCount);

			v += fullMatrices[boneIndex].MultiplyPoint3(*vertices) * boneInfluence.weight[j];

			w += boneInfluence.weight[j];
		}

		Assert(w > 0.99f && w < 1.01f);

		minMaxAABB.Encapsulate(v);
	}

	output = minMaxAABB;

	return true;
}
#endif

bool SkinnedMeshRenderer::CalculateAnimatedPosesWithRoot (const Matrix4x4f& rootMatrix, Matrix4x4f* poses, size_t size)
{
	if (!CalculateAnimatedPoses(poses, size))
		return false;

	for (int i=0;i<size;i++)
	{
		Matrix4x4f temp;
		MultiplyMatrices4x4(&rootMatrix, poses + i, &temp);
		poses[i] = temp;
	}
	return true;
}

bool SkinnedMeshRenderer::CalculateSkinningMatrices (const Matrix4x4f& rootPose, Matrix4x4f* skinnedPoses, size_t size)
{
	Assert(m_CachedMesh != NULL);
	DebugAssert (size > 0);

	if (!CalculateAnimatedPoses(skinnedPoses, size))
		return false;

	MultiplyMatrixArrayWithBase4x4 (&rootPose, skinnedPoses, m_CachedMesh->GetBindposes(), skinnedPoses, size);

	return true;
}

bool SkinnedMeshRenderer::CalculateAnimatedPoses (Matrix4x4f* poses, size_t size)
{
	if (IsOptimized())
	{
		const dynamic_array<UInt16>& skeletonIndices = GetSkeletonIndices();
		if (!skeletonIndices.empty()) // implies: GetAnimator() && GetAnimationInterface()
			return GetAnimationInterface()->CalculateWorldSpaceMatricesMainThread(*GetAnimator(), skeletonIndices.begin(), size, poses);
		else
			return false;
	}
	else
	{
		if (size > m_Bones.size())
			return false;

		bool hasAnyBones = false;
		for (int i=0;i<size;i++)
		{
			Transform* transform = m_Bones[i];
			if (transform)
			{
				poses[i] = transform->GetLocalToWorldMatrix ();
				hasAnyBones = true;
			}
			else
				poses[i].SetIdentity ();
		}
		
		return hasAnyBones;
	}
}

const dynamic_array<UInt16>& SkinnedMeshRenderer::GetSkeletonIndices()
{
	Assert(IsOptimized());

	if (m_SkeletonIndices.empty())
		CreateCachedAnimatorBinding ();


	return m_SkeletonIndices;
}

int SkinnedMeshRenderer::GetBindposeCount () const
{
	return m_CachedMesh ? m_CachedMesh->GetBindposeCount() : 0;
}

bool SkinnedMeshRenderer::PrepareVBO(bool hasSkin, bool hasBlendshape, bool doMemExport, int flags)
{
	if (!hasSkin && !hasBlendshape)
		return false;

	// Right now, it can be false for 2 cases:
	// 1. GetSkinnedVerticesAndNormal
	//		In this case, SF_ReadbackBack will also be set.
	//		SkinMeshInfo.outVertices will be allocated in SkinnedMeshRenderer.m_SkinnedVertices.
	// 2. Bake mesh
	//		In this case, SkinMeshInfo.outVertices will be the vertex data of the output Mesh.
	bool	doNeedVBO = !(flags & SF_NoUpdateVBO);
	bool	newVBO = false;
	
	if (hasBlendshape)
	{
		// Handle cases:
		// hasSkin==true, hasBlendshape==true or
		// hasSkin==false, hasBlendshape==true
		//
		// Hardware skinning is not applicable
		if (!m_VBO && doNeedVBO)
		{
			m_VBO = GetGfxDevice().CreateVBO();
			m_VBO->SetVertexStreamMode(0, VBO::kStreamModeDynamic);
			m_VBO->SetIndicesDynamic(false);
			newVBO = true;
		}
	}
	else
	{
		// Handle case: hasSkin==ture, hasBlendshape==false
		bool canMapVBO = m_VBO && (m_VBO->GetVertexStreamMode(0) != VBO::kStreamModeNoAccess);
		
		// Check if mem-export qualification changes. See SkinnedMeshRenderer::PrepareSkinXenon
		// Re-create VBO if it changes.
		if (m_VBO && (doMemExport == canMapVBO))
		{
			GetGfxDevice().DeleteVBO(m_VBO);
			m_VBO = NULL;
			if(m_MemExportInfo)
				m_MemExportInfo->SetDestVBO(NULL);
		}

		if (!m_VBO && doNeedVBO)
		{
			m_VBO = GetGfxDevice().CreateVBO();
			if (!doMemExport)
			{
				m_VBO->SetVertexStreamMode(0, VBO::kStreamModeDynamic);
				m_VBO->SetIndicesDynamic(false);
			}
			else
				m_VBO->UseAsStreamOutput();
			newVBO = true;
		}
	}

	if (doMemExport)
	{
		// Rebuild the VBO in case mem-export qualification is gained. This could use cleaning up.
		// OR if source is dirty.
		if (newVBO || (doNeedVBO && m_SourceMeshDirty))
			m_CachedMesh->CopyToVBO(m_ChannelsInVBO, *m_VBO);
		return true;
	}

	if (newVBO || (doNeedVBO && (m_SourceMeshDirty || m_VBO->IsVertexBufferLost())))
	{
		// fill the VBO
		m_VBO->SetMappedFromRenderThread(!m_Cloth);
		m_CachedMesh->CopyToVBO(m_ChannelsInVBO, *m_VBO);
		m_SourceMeshDirty = false;
	}

	return true;
}


bool SkinnedMeshRenderer::PrepareSkinCommon(UInt32 requiredChannels, int flags, SkinMeshInfo& skin, CalculateSkinMatricesTask* calcSkinMatricesTask)
{
	m_UpdateBeforeRendering = false;
	if (!m_CachedMesh || (m_CachedMesh->GetSubMeshCount () == 0))
		return false;
	DebugAssertIf(m_CachedMesh != m_Mesh);

	int bindposeCount = GetBindposeCount();
	int blendShapeCount = GetValidBlendShapeWeightCount ();

	if (bindposeCount > 0 && m_CachedMesh->GetMaxBoneIndex() >= bindposeCount)
	{
		ErrorStringObject("Bone influences do not match bones.", this);
		return false;
	}

	// We are not monitoring whether the bones have changed
	// so we always have to make cached surface area dirty,
	// as it could have changed.
	SET_CACHED_SURFACE_AREA_DIRTY();

	bool hasSkin = (bindposeCount > 0) && !m_CachedMesh->GetSkin().empty();

	// For skinned meshes we only care about active blend shapes.
	// For non-skinned meshes we need to take the blend shape code path
	// even if all the shapes are zero weight. There is no optimized path
	// to render the undeformed source mesh (case 557165).
	// TODO: Write an optimized path for no skin, no active blend shapes.
	bool hasBlendshape = false;
	if (hasSkin)
		hasBlendshape = blendShapeCount > 0;
	else 
		hasBlendshape = m_CachedBlendShapeCount > 0;

	bool doMemExport = DoesQualifyForMemExport() &&
		(flags & SF_AllowMemExport) && hasSkin && !hasBlendshape;

	m_CachedMesh->InitVertexBufferData(requiredChannels);
	m_ChannelsInVBO = m_CachedMesh->GetAvailableChannels();

	if (!PrepareVBO(hasSkin, hasBlendshape, doMemExport, flags))
		return false;

	// Fill SkinMeshInfo
	skin.boneCount = bindposeCount;
	skin.blendshapeCount = blendShapeCount;
	skin.vertexCount = m_CachedMesh->GetVertexCount();
	skin.memExport = doMemExport;
	skin.Allocate();

	if (hasSkin)
	{
		skin.bonesPerVertex = GetBonesPerVertexCount();
		skin.compactSkin = m_CachedMesh->GetSkinInfluence(skin.bonesPerVertex);

		Matrix4x4f rootPose;
		if (!(flags & SF_ClothPlaying))
			rootPose = GetActualRootBone().GetWorldToLocalMatrixNoScale ();
		else
			// clothed skins are simulated using world space rotation, so rotating the character will affect the cloth simulation.
			// translation is applied using forces in the cloth, which is smoother.
			rootPose.SetTranslate (-GetActualRootBone().GetPosition());

		bool canCalcSkinMatricesInMT = false;
		if (calcSkinMatricesTask && IsOptimized())
		{
			const dynamic_array<UInt16>& skeletonIndices = GetSkeletonIndices();
			if (skeletonIndices.empty())
				return false;
			else  // implies GetAnimationInterface() && GetAnimator()
			{
				const void* skeletonPose = GetAnimationInterface()->GetGlobalSpaceSkeletonPose(*GetAnimator());
				if (skeletonPose)
				{
					canCalcSkinMatricesInMT = true;

					calcSkinMatricesTask->skeletonPose = skeletonPose;
					calcSkinMatricesTask->skeletonIndices = skeletonIndices.begin();
					calcSkinMatricesTask->rootPose = rootPose;
					calcSkinMatricesTask->bindPoseCount = bindposeCount;
					calcSkinMatricesTask->bindPose = m_CachedMesh->GetBindposes();

					calcSkinMatricesTask->outPose = skin.cachedPose;
				}
			}
		}

		if (!canCalcSkinMatricesInMT)
		{
			// slow code path
			if (!CalculateSkinningMatrices(rootPose, skin.cachedPose, bindposeCount))
				return false;
		}
	}
	else
	{
		skin.cachedPose = NULL;
		skin.compactSkin = NULL;
	}

	if (hasBlendshape)
	{
		Assert (skin.blendshapeCount <= m_CachedMesh->GetBlendShapeChannelCount());
		memcpy (skin.blendshapeWeights, m_BlendShapeWeights.begin(), skin.blendshapeCount * sizeof(float));
		skin.blendshapes = &m_CachedMesh->GetBlendShapeData();
	}

#if UNITY_PS3
	if ((NULL == m_Cloth) && (!m_CachedMesh->m_PartitionInfos.empty()))
	{
		m_ChannelsInVBO = m_CachedMesh->GetAvailableChannels();
		return true;
	}
#endif

	const VertexData& vertexData = m_CachedMesh->GetVertexData();
	const StreamInfo streamInfo = vertexData.GetStream(0);
	skin.inVertices = vertexData.GetDataPtr() + streamInfo.offset;
	skin.inStride = streamInfo.stride;
	skin.outStride = streamInfo.stride;
#if !UNITY_FLASH
	if (streamInfo.channelMask & ~VERTEX_FORMAT3(Vertex, Normal, Tangent))
		ErrorString(Format("Skinned mesh stream should contain only positions, normals and tangents. channelMask was %lx", streamInfo.channelMask));
#endif

	if (skin.memExport)
		return true;

	// Skin vertices into vbo
	const ChannelInfo& normalInfo = vertexData.GetChannel(kShaderChannelNormal);
	const ChannelInfo& tangentInfo = vertexData.GetChannel(kShaderChannelTangent);

#if !UNITY_PS3 // Stream layout for skinned models is different on PS3
	DebugAssert(!normalInfo.IsValid() || (normalInfo.stream == 0 && normalInfo.format == kChannelFormatFloat && normalInfo.dimension == 3));
	DebugAssert(!tangentInfo.IsValid() || (tangentInfo.stream == 0 && tangentInfo.format == kChannelFormatFloat && tangentInfo.dimension == 4));
#endif

	skin.skinNormals = normalInfo.IsValid();
	skin.normalOffset = normalInfo.offset;
	skin.skinTangents = tangentInfo.IsValid();
	skin.tangentOffset = tangentInfo.offset;

	if (flags & SF_ReadbackBuffer)
	{
		// Allocate a temporary buffer to read back from
		m_SkinnedVertices.resize_uninitialized(skin.outStride * skin.vertexCount);
		skin.outVertices = &m_SkinnedVertices[0];
	}
	return true;
}

#if UNITY_PS3
bool SkinnedMeshRenderer::PrepareSkinPS3( UInt32 requiredChannels, int flags, SkinMeshInfo& skin, CalculateSkinMatricesTask* calcSkinMatricesTask )
{
	if (!m_CachedMesh || m_CachedMesh->GetSkin().empty ())
		return false;

	skin.vertexData = NULL;

	if (!PrepareSkinCommon( requiredChannels, flags, skin, calcSkinMatricesTask ))
		return false;

	skin.vertexData = &m_CachedMesh->GetVertexData();

	if(m_CachedMesh->m_PartitionInfos.empty() || m_Cloth)
		return true;

	if(m_SourceMeshDirty)
	{
		VertexBufferData vertexBuffer;
		IndexBufferData indexBuffer;

		m_CachedMesh->GetVertexBufferData( vertexBuffer, m_ChannelsInVBO );
		m_CachedMesh->GetIndexBufferData( indexBuffer );

		vertexBuffer.inflPerVertex = skin.bonesPerVertex;
		vertexBuffer.numInfluences = m_CachedMesh->GetSkin().size();
		vertexBuffer.influences = skin.compactSkin;
		vertexBuffer.numBones = skin.boneCount;
		vertexBuffer.bones = skin.cachedPose;

		m_VBO->UpdateVertexData( vertexBuffer );
		m_VBO->UpdateIndexData( indexBuffer );

		m_SourceMeshDirty = false;
	}
	else
		((GfxGCMVBO*)m_VBO)->UpdateBones(skin.boneCount, skin.cachedPose);

	skin.Release();
	return false;
}
#endif

bool SkinnedMeshRenderer::PrepareSkinGPU( UInt32 requiredChannels, int flags, SkinMeshInfo& skin, CalculateSkinMatricesTask* calcSkinMatricesTask )
{
	if (!PrepareSkinCommon( requiredChannels, flags, skin, calcSkinMatricesTask ))
		return false;

	if (skin.memExport)
	{
		Assert(m_ChannelsInVBO != 0);
		Assert(skin.inStride == skin.outStride);


		// TODO: Are there any situations where this might change on the fly? In the editor?
		if(!m_MemExportInfo->GetDestVBO() || m_SourceMeshDirty)
		{
		VertexBufferData vertexBuffer;
		const StreamInfo& skinStream = vertexBuffer.streams[0];

		m_CachedMesh->GetVertexBufferData(vertexBuffer, m_ChannelsInVBO);

		// Source data
		const size_t dataBufferSize = skinStream.stride * skin.vertexCount;
		const void* dataBufferPtr = vertexBuffer.buffer + skinStream.offset;

		// Skin
		Assert(sizeof(BoneInfluence) == 32);
		const int skinBufferSize = m_CachedMesh->GetVertexCount() * sizeof(BoneInfluence);
			//		const BoneInfluence* skinBufferPtr = m_CachedMesh->GetBoneWeights();
			void *skinBufferPtr = m_CachedMesh->GetSkinInfluence(skin.bonesPerVertex);

			m_MemExportInfo->SetVertexCount(skin.vertexCount);
			m_MemExportInfo->SetChannelMap(skinStream.channelMask);
			m_MemExportInfo->SetStride(skin.outStride);
			m_MemExportInfo->SetDestVBO(m_VBO);
			m_MemExportInfo->SetBonesPerVertex(skin.bonesPerVertex);

			GetGfxDevice().UpdateSkinSourceData(m_MemExportInfo, dataBufferPtr, (const BoneInfluence *)skinBufferPtr, m_SourceMeshDirty);
		}


		// Bones (uses 4 weights, ignores quality settings)
		Assert(skin.boneCount > 0);

		skin.mei = m_MemExportInfo;
		m_SourceMeshDirty = false;
	}
	else
	{
		Assert(m_SourceMeshDirty == false);
	}

	return true;
}

bool SkinnedMeshRenderer::PrepareSkin( UInt32 requiredChannels, int flags, SkinMeshInfo& skin, CalculateSkinMatricesTask* calcSkinMatricesTask )
{
#if UNITY_PS3
	return PrepareSkinPS3(requiredChannels, flags, skin, calcSkinMatricesTask);
#else
	return PrepareSkinGPU(requiredChannels, flags, skin, calcSkinMatricesTask);
#endif
}

#if UNITY_EDITOR
void SkinnedMeshRenderer::UpdateClothDataForEditing(const SkinMeshInfo& skin)
{
	// update cloth vertices in edit mode for the cloth vertex editor
	if (m_Cloth != NULL && !IsWorldPlaying())
	{
		dynamic_array<Vector3f> &vertices = m_Cloth->GetVertices();
		dynamic_array<Vector3f> &normals = m_Cloth->GetNormals();
		GetSkinnedVerticesAndNormals (&vertices, &normals);
	}
}
#endif

bool SkinnedMeshRenderer::SkinMesh( SkinMeshInfo& skin, bool lastMemExportThisFrame, UInt32 cpuFence, int flags )
{
	GfxDevice& device = GetGfxDevice();
	if (skin.memExport)
	{
		GetGfxDevice().UpdateSkinBonePoses(m_MemExportInfo, skin.boneCount, skin.cachedPose);
		skin.Release();

		// Issue GPU skinning requests in sync. Cloth is done on CPU in parallel.
		PROFILER_AUTO(gMeshSkinningSkinGPU, this)
		device.SkinOnGPU(m_MemExportInfo, lastMemExportThisFrame);
		device.GetFrameStats().AddDrawCall (skin.vertexCount, skin.vertexCount);
		GPU_TIMESTAMP();
		return true;
	}
	else
	{
#if ENABLE_MULTITHREADED_CODE
		m_CachedMesh->SetCurrentCPUFence(cpuFence);
#endif
		return GetGfxDevice().SkinMesh(skin, (flags & SF_ReadbackBuffer) ? NULL : m_VBO);
	}
}

bool SkinnedMeshRenderer::SkinMeshImmediate( UInt32 requiredChannels )
{
	GfxDevice& device = GetGfxDevice();
	// Double check there are no fences inserted during skinning
	UInt32 expectedFence = device.GetNextCPUFence();
	device.BeginSkinning(1);
	SkinMeshInfo skin;
	int flags = SF_AllowMemExport;
	bool success = PrepareSkin(requiredChannels, flags, skin);
	if (success)
	{
		SkinMesh(skin, true, expectedFence, flags);
#if UNITY_EDITOR
		UpdateClothDataForEditing(skin);
#endif
	}
	device.EndSkinning();
	// Insert fence after all skinning is complete
	UInt32 fence = device.InsertCPUFence();

	return success;
}

#if UNITY_EDITOR
float SkinnedMeshRenderer::GetCachedSurfaceArea ()
{
	if (m_CachedSurfaceArea >= 0.0f)
		return m_CachedSurfaceArea;

	Mesh* mesh = m_CachedMesh;
	if (!mesh)
	{
		m_CachedSurfaceArea = 1.0f;
		return m_CachedSurfaceArea;
	}

	Matrix4x4f objectToWorld = GetTransformInfo ().worldMatrix;

	Mesh::TemporaryIndexContainer triangles;
	mesh->GetTriangles (triangles);

	dynamic_array<Vector3f> vertices;
	if (GetSkinnedVerticesAndNormals (&vertices, NULL)) // this may fail and return empty array as is the case in repro for bug 505751.
	{
		m_CachedSurfaceArea = CalculateSurfaceArea (objectToWorld, triangles, vertices);
	}
	else
	{
		m_CachedSurfaceArea = 1.0f; // to avoid repeat invocation of failing GetCachedSurfaceArea
	}
	return m_CachedSurfaceArea;
}

bool SkinnedMeshRenderer::GetSkinnedVerticesAndNormals (dynamic_array<Vector3f>* vertices, dynamic_array<Vector3f>* normals)
{
	UInt32 requiredChannels = (1 << kShaderChannelVertex);
	if (normals)
		requiredChannels |= (1 << kShaderChannelNormal);

	SkinMeshInfo skin;
	if (!PrepareSkinCommon(requiredChannels, SF_ReadbackBuffer | SF_NoUpdateVBO, skin))
		return false;

	DeformSkinnedMesh(skin);
	skin.Release();

	bool hasVertices = m_ChannelsInVBO & (1 << kShaderChannelVertex);
	bool hasNormals = skin.skinNormals;

	if (vertices && hasVertices)
	{
		vertices->resize_uninitialized (skin.vertexCount);

		for (int i = 0; i < skin.vertexCount; i++)
		{
			char* vertex = ((char*)skin.outVertices + i * skin.outStride);
			(*vertices)[i] = *((Vector3f*)vertex);
		}
	}

	if (normals && hasNormals)
	{
		normals->resize_uninitialized (skin.vertexCount);

		for (int i = 0; i < skin.vertexCount; i++)
		{
			char* normal = ((char*)skin.outVertices + i * skin.outStride + skin.normalOffset);
			(*normals)[i] = *((Vector3f*)normal);
		}
	}
	return true;
}
#endif


int SkinnedMeshRenderer::GetBonesPerVertexCount ()
{
	if (m_Quality == 0)
		return GetQualitySettings().GetCurrent().blendWeights;
	else
		return m_Quality;
}

template<class TransferFunction> inline
void SkinnedMeshRenderer::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	transfer.SetVersion(2);

	TRANSFER_SIMPLE (m_Quality);
	TRANSFER_SIMPLE (m_UpdateWhenOffscreen);
	transfer.Align();

	transfer.Transfer (m_Mesh, "m_Mesh");

	transfer.Transfer (m_Bones, "m_Bones", kHideInEditorMask);
	transfer.Align();	

	transfer.Transfer(m_BlendShapeWeights, "m_BlendShapeWeights");
	
	transfer.Transfer(m_RootBone, "m_RootBone");

	transfer.Transfer(m_AABB, "m_AABB");
	transfer.Transfer(m_DirtyAABB, "m_DirtyAABB", kHideInEditorMask);
	transfer.Align();
}

IMPLEMENT_CLASS_HAS_INIT (SkinnedMeshRenderer)
IMPLEMENT_OBJECT_SERIALIZE (SkinnedMeshRenderer)

void SkinnedMeshRenderer::InitializeClass ()
{
	REGISTER_MESSAGE_VOID (SkinnedMeshRenderer, kBecameVisible, BecameVisible);
	REGISTER_MESSAGE_VOID (SkinnedMeshRenderer, kBecameInvisible, BecameInvisible);

	REGISTER_MESSAGE_VOID(SkinnedMeshRenderer, kDidDeleteMesh, DidDeleteMesh);
	REGISTER_MESSAGE_VOID(SkinnedMeshRenderer, kDidModifyMesh, DidModifyMesh);
	RegisterAllowNameConversion (SkinnedMeshRenderer::GetClassStringStatic(), "m_LodMesh", "m_Mesh");
	RegisterAllowNameConversion (SkinnedMeshRenderer::GetClassStringStatic(), "m_Animation", "m_DisableAnimationWhenOffscreen");
	
	
	InitializeBlendShapeAnimationBindingInterface ();

}

void SkinnedMeshRenderer::CleanupClass ()
{
	CleanupBlendShapeAnimationBindingInterface ();
}

void SkinnedMeshRenderer::DidDeleteMesh ()
{
	m_CachedMesh = NULL;
	m_CachedBlendShapeCount = 0;
}

void SkinnedMeshRenderer::DidModifyMesh ()
{
	m_SourceMeshDirty = true;
}

void SkinnedMeshRenderer::UpdateCachedMesh ()
{
	Mesh* mesh = m_Mesh;
	if (mesh != m_CachedMesh)
	{
		m_SourceMeshDirty =  true;

		m_CachedMesh = mesh;
		BoundsChanged();
		m_TransformDirty = true;
		SET_CACHED_SURFACE_AREA_DIRTY();

		m_MeshNode.RemoveFromList();

		if (m_CachedMesh)
			m_CachedMesh->AddObjectUser( m_MeshNode );
	}
	
	if (m_CachedMesh != NULL)
		m_CachedBlendShapeCount = GetBlendShapeChannelCount(m_CachedMesh->GetBlendShapeData());
	else
		m_CachedBlendShapeCount = 0;

	ClearCachedAnimatorBinding();
}



void SkinnedMeshRenderer::UpdateRenderer()
{
	if (GetEnabled() && IsActive())
	{
		// Force update bounding volumes when we have a root bone or we have to
		bool recalculateBoundingVolumeEveryFrame = ShouldRecalculateBoundingVolumeEveryFrame();
		if (recalculateBoundingVolumeEveryFrame)
		{
			// Make we continously get this callback every frame so that we can make sure that skinned meshes are put in the gActiveSkinnedMeshes queue
			// depending on their visibility
			UpdateManagerState(true);

			// The root transform can move every frame and there is no way for us to track it.
			TransformChanged (Transform::kPositionChanged | Transform::kRotationChanged | Transform::kScaleChanged);
		}
	}

	UpdateVisibleSkinnedMeshQueue(IsActive());

	Super::UpdateRenderer();
}

void SkinnedMeshRenderer::UpdateVisibleSkinnedMeshQueue (bool active)
{
	bool needsUpdate = m_Visible && (GetEnabled() && active);
	if (needsUpdate == m_SkinNode.IsInList())
		return;

	if (needsUpdate)
		gActiveSkinnedMeshes.push_back(m_SkinNode);
	else
		m_SkinNode.RemoveFromList();
}

void SkinnedMeshRenderer::Deactivate (DeactivateOperation operation)
{
	Super::Deactivate(operation);
	UpdateVisibleSkinnedMeshQueue(false);
	ClearCachedAnimatorBinding();
}

void SkinnedMeshRenderer::SetUpdateWhenOffscreen (bool onlyIfVisible)
{
	m_UpdateWhenOffscreen = onlyIfVisible;

	// We might have to start firing UpdateRenderer events every frame...
	UpdateManagerState (IsActive());
	BoundsChanged ();

	SetDirty();
}

void SkinnedMeshRenderer::BecameVisible ()
{
	m_Visible = true;

	// When using LOD we might have a skinned mesh, the animation component might sample during OnBecameVisible since it wasn't visible before.
	// In that case the root bone might change, thus we force a transformDirty in on became visible as well as every single frame.
	if (ShouldRecalculateBoundingVolumeEveryFrame())
		m_TransformDirty = true;

	UpdateVisibleSkinnedMeshQueue (IsActive());

	m_UpdateBeforeRendering = true;
}

void SkinnedMeshRenderer::BecameInvisible ()
{
	m_Visible = false;
	UpdateVisibleSkinnedMeshQueue (IsActive());
}


static GPUSkinningInfo* CreateGPUSkinningIfAvailable()
{
	if (!GetBuildSettings().hasAdvancedVersion)
		return NULL;

	return GetGfxDevice().CreateGPUSkinningInfo();
}


void SkinnedMeshRenderer::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	if (!m_MemExportInfo)
		m_MemExportInfo = CreateGPUSkinningIfAvailable();

	#if UNITY_EDITOR || SUPPORT_REPRODUCE_LOG
	HandleOldSkinnedFilter ();
	#endif

	Super::AwakeFromLoad(awakeMode);

	UpdateCachedMesh ();

	// Make sure we are added to the visibile queue if we the renderer is active
	UpdateVisibleSkinnedMeshQueue (IsActive());

	// The root transform pptr might have changed and thus worldTransform needs to be updated
	TransformChanged(Transform::kPositionChanged | Transform::kRotationChanged | Transform::kScaleChanged | Transform::kParentingChanged);

	m_BlendShapeWeights.resize_initialized(m_CachedBlendShapeCount, 0.0F);
}

void SkinnedMeshRenderer::SetQuality (int quality)
{
	m_Quality = quality; SetDirty();
}

void SkinnedMeshRenderer::Render (int subsetIndex, const ChannelAssigns& channels)
{
	PROFILER_AUTO(gMeshSkinningRender, this)

	if (m_CachedMesh)
	{
		if (m_CachedMesh->GetSkin().empty() && m_CachedBlendShapeCount == 0)
		{
			ErrorStringObject("SkinnedMeshRenderer requires a mesh with skinning or blendshape information.", this);
			return;
		}

		UInt32 requiredChannels = channels.GetSourceMap();
		// Skinned cloth prefers to have normals, even if shader does not need them
		if (m_Cloth)
			requiredChannels |= (1<<kShaderChannelNormal);

		// Just in time update
		if (m_UpdateBeforeRendering || m_SourceMeshDirty || !m_VBO || m_VBO->IsVertexBufferLost())
		{
			PROFILER_BEGIN(gMeshSkinningUpdateImmediate, this)
			bool success = SkinMeshImmediate(requiredChannels);
			PROFILER_END

			// Mesh skinning can fail (Bone indices out of bounds, bone transforms missing etc)
			if (!success)
				return;
		}

		if (m_CustomProperties)
			GetGfxDevice().SetMaterialProperties (*m_CustomProperties);
		DrawUtil::DrawVBOMeshRaw (*m_VBO, *m_CachedMesh, channels, subsetIndex, m_ChannelsInVBO);
	}

	GPU_TIMESTAMP();
}

void SkinnedMeshRenderer::SetLocalAABB(const AABB& bounds)
{
	m_AABB = bounds;
	m_DirtyAABB = false;
	SetDirty();
}

void SkinnedMeshRenderer::ReadSkinningDataForCloth(const SkinMeshInfo& skin)
{
#if ENABLE_CLOTH
	void *normalPointer = (char*)skin.outVertices + skin.normalOffset;
	if (!skin.skinNormals)
		normalPointer = NULL;
	void *tangentPointer = (char*)skin.outVertices + skin.tangentOffset;
	if (!skin.skinTangents)
		tangentPointer = NULL;

	GetIPhysics()->SetUpSkinnedBuffersOnSkinnedCloth(*m_Cloth, skin.outVertices, normalPointer, tangentPointer, skin.outStride);
#endif
}

void SkinnedMeshRenderer::UpdateAllSkinnedMeshes(UpdateType updateType, dynamic_array<SkinnedMeshRenderer*>* outMeshes)
{
	PROFILER_AUTO(gMeshSkinningUpdate, NULL)

	// TODO: we should submit skinning jobs, go do something else (e.g. update particles, movies, whatnot),
	// and wait/reintegrate jobs after that is done.

	int flags = SF_None;
	if (updateType == kUpdateCloth)
	{
		flags |= SF_ReadbackBuffer;
		if (IsWorldPlaying())
			flags |= SF_ClothPlaying;
	}
	else
	{
		flags = SF_AllowMemExport;
	}

	size_t skinCount = 0;
	size_t maxCount = gActiveSkinnedMeshes.size_slow();
	dynamic_array<SkinnedMeshRenderer*> skinMeshes(maxCount, kMemTempAlloc);
	dynamic_array<SkinMeshInfo> skinInfos(maxCount, kMemTempAlloc);
	dynamic_array<CalculateSkinMatricesTask> calculateSkinMatricesTasks(maxCount, kMemTempAlloc);

	PROFILER_BEGIN(gMeshSkinningPrepare, NULL);
	// Find out which renderers to skin this frame
	const SkinMeshInfo* lastMemExport = 0;
	SkinnedMeshList::iterator next;
	int skinMatrixTaskCount = 0;
	for (SkinnedMeshList::iterator i=gActiveSkinnedMeshes.begin();i != gActiveSkinnedMeshes.end();i=next)
	{
		SkinnedMeshRenderer& skin = **i;
		next = i;
		next++;
		UpdateType type = (skin.m_Cloth && IsWorldPlaying())? kUpdateCloth : kUpdateNonCloth;
		if (type != updateType)
			continue;
		SkinMeshInfo& info = skinInfos[skinCount];
		memset(&info, 0, sizeof(SkinMeshInfo));

		calculateSkinMatricesTasks[skinMatrixTaskCount].skeletonPose = NULL; // mark invalid task
		if (skin.PrepareSkin(skin.m_ChannelsInVBO, flags, info, &calculateSkinMatricesTasks[skinMatrixTaskCount]))
		{
			if (calculateSkinMatricesTasks[skinMatrixTaskCount].skeletonPose != NULL)
				skinMatrixTaskCount++; // valid task
			skinMeshes[skinCount] = &skin;
			skinCount++;
			if (info.memExport)
				lastMemExport = &info;
		}
	}
	PROFILER_END;

	if (skinMatrixTaskCount)
	{
#if ENABLE_MULTITHREADED_CODE
		JobScheduler& scheduler = GetJobScheduler();
		JobScheduler::JobGroupID jobGroup;
#define	CALC_SKIN_MATRICES_LOOP(x,list,size) \
		{ \
		size_t jobCount = size; \
		jobGroup = scheduler.BeginGroup(jobCount);	\
		for (size_t i = 0; i < jobCount; ++i) \
		{ CalculateSkinMatricesTask& task = list[i]; scheduler.SubmitJob (jobGroup, x, &task, NULL); } \
		scheduler.WaitForGroup (jobGroup); \
		}
#else
#define	CALC_SKIN_MATRICES_LOOP(x,list,size) \
	for (size_t i=0;i<size;i++) \
		{ CalculateSkinMatricesTask& task = list[i]; x (&task); } 
#endif	

		// Do it in multiple threads
		if (GetAnimationInterface())
		{
			/// @TODO: Simplify this. No need for having an interface that returns a callback... Doh
			CalculateAnimatorSkinMatricesFunc calculateAnimatorSkinMatricesFunc =
				GetAnimationInterface()->GetCalculateAnimatorSkinMatricesFunc();
			if (calculateAnimatorSkinMatricesFunc)
				CALC_SKIN_MATRICES_LOOP(calculateAnimatorSkinMatricesFunc, calculateSkinMatricesTasks, skinMatrixTaskCount)
		}
	}

	if (skinCount == 0)
		return;

	skinMeshes.resize_uninitialized(skinCount);
	skinInfos.resize_uninitialized(skinCount);

	// Double check there are no fences inserted during skinning
	GfxDevice& device = GetGfxDevice();
	UInt32 expectedFence = device.GetNextCPUFence();

	// Now we know exactly which renderers to skin and which one is last
	device.BeginSkinning(skinCount);
	for (int i = 0; i < skinCount; i++)
	{
		SkinMeshInfo& info = skinInfos[i];
		SkinnedMeshRenderer& skin = *skinMeshes[i];
		bool lastMemExportThisFrame = (&info == lastMemExport);
		skin.SkinMesh(info, lastMemExportThisFrame, expectedFence, flags);
	}

	PROFILER_BEGIN(gMeshSkinningWait, NULL)
	device.EndSkinning();
	PROFILER_END;

	// Insert fence after all skinning is complete
	UInt32 fence = device.InsertCPUFence();
	Assert(fence == expectedFence);

	// Read back vertices for cloth
	// It's fine to do this after EndSkinning() since we own the buffer (m_SkinnedVertices)
	if (updateType == kUpdateCloth)
	{
		for (int i = 0; i < skinCount; i++)
		{
			SkinnedMeshRenderer& skin = *skinMeshes[i];
			skin.ReadSkinningDataForCloth(skinInfos[i]);
		}
	}

	if (outMeshes)
		outMeshes->assign(skinMeshes.begin(), skinMeshes.end());
}

void SkinnedMeshRenderer::UploadSkinnedClothes(const dynamic_array<SkinnedMeshRenderer*>& skinnedMeshes)
{
	int skinCount = skinnedMeshes.size();
	for (int i = 0; i < skinCount; i++)
	{
		SkinnedMeshRenderer& skin = *skinnedMeshes[i];

		if (skin.m_SkinnedVertices.empty())
			continue;

		VertexStreamData mappedVSD;
		if (skin.m_VBO->MapVertexStream(mappedVSD, 0))
		{
			memcpy(mappedVSD.buffer, skin.m_SkinnedVertices.data(), skin.m_SkinnedVertices.size());
			skin.m_VBO->UnmapVertexStream(0);
		}
	}
}

Transform& SkinnedMeshRenderer::GetActualRootBone ()
{
	Transform* rootBone = m_RootBone;
	if (rootBone != NULL)
		// Not optimized mode && m_RootBone != NULL
		return *rootBone;
	else
		// Optimized mode
		// Not optimized mode && m_RootBone == NULL
		return GetTransform();
}


bool SkinnedMeshRenderer::CalculateRootLocalSpaceBounds (MinMaxAABB& minMaxAAbb)
{
	Matrix4x4f* poses;
	int poseCount = GetBindposeCount();
	ALLOC_TEMP(poses, Matrix4x4f, poseCount);

	Transform& rootBone = GetActualRootBone();

	if (CalculateAnimatedPosesWithRoot(rootBone.GetWorldToLocalMatrix(), poses, poseCount) && CalculateBoneBasedBounds(poses, poseCount, minMaxAAbb))
		return true;
	else
		return false;
}

void SkinnedMeshRenderer::UpdateTransformInfo ()
{
	Transform& rootBone = GetActualRootBone();

	Vector3f pos;
	Quaternionf rot;
	TransformType transformType = rootBone.GetPositionAndRotationWithTransformType (pos, rot);

	const bool hasSkin = !m_CachedMesh ? true : !m_CachedMesh->GetSkin().empty();
	if (hasSkin || IsNoScaleTransform(transformType))
	{
		m_TransformInfo.transformType = transformType & kOddNegativeScaleTransform;
		m_TransformInfo.worldMatrix.SetTR (pos, rot);
		m_TransformInfo.invScale = 1.0f;
	}
	// This codepath only exists for blendshapes. Skinned meshes will always use only position & rotation (no scale)
	// For blendshapes the skinning code does no bone or matrix deformation thus there is no good way to plug scale in there.
	else
	{
		float uniformScale = 1.0f;
		m_TransformInfo.worldMatrix = rootBone.GetLocalToWorldMatrix();
		m_TransformInfo.transformType = transformType = ComputeTransformType(m_TransformInfo.worldMatrix, uniformScale);
		m_TransformInfo.invScale = 1.0f / uniformScale;
	}

#if UNITY_SUPPORTS_VFP || (UNITY_SUPPORTS_NEON && !UNITY_DISABLE_NEON_SKINNING)
	// NOTE: optimized VFP routines do not do any normalization
	// instead we rely on GPU to do that
	if (GetBonesPerVertexCount () != 1)
		m_TransformInfo.transformType |= kNonUniformScaleTransform;
#endif

	// Compute world space bounding volume from each bone to get a very accurate bounding volume
	if (m_UpdateWhenOffscreen && hasSkin)
	{
		Matrix4x4f* poses;
		int poseCount = GetBindposeCount();
		ALLOC_TEMP(poses, Matrix4x4f, poseCount);
		
		MinMaxAABB accurateMinMax;
		if (CalculateAnimatedPoses(poses, poseCount) && CalculateBoneBasedBounds(poses, poseCount, accurateMinMax))
		{
			m_TransformInfo.worldAABB = accurateMinMax;
			InverseTransformAABB(m_TransformInfo.worldAABB, pos, rot, m_TransformInfo.localAABB);
			return;
		}
	}

	// Calculate m_AABB from the current pose and undirty the AABB
	if (m_DirtyAABB)
	{
		MinMaxAABB accurateMinMax;
		if (!hasSkin && m_CachedMesh)
		{
			accurateMinMax = m_CachedMesh->GetLocalAABB();
			SetLocalAABB(accurateMinMax);
		}
		else if (CalculateRootLocalSpaceBounds(accurateMinMax))
		{
			SetLocalAABB(accurateMinMax);
		}
		else
		{
			////@TOOD: figure out something logicalll....
			m_AABB = AABB(Vector3f::zero, Vector3f::zero);
		}
	}

	MinMaxAABB localAABB = m_AABB;

	if (IsNoScaleTransform(transformType))
	{
		m_TransformInfo.localAABB = localAABB;
		TransformAABB (localAABB, pos, rot, m_TransformInfo.worldAABB);
	}
	else
	{
		// Calculate world space bounding volume (Transform from root space to world space)
		Matrix4x4f rootToWorldMatrix;
		rootBone.CalculateTransformMatrix (rootToWorldMatrix);
		TransformAABB (localAABB, rootToWorldMatrix, m_TransformInfo.worldAABB);


		// Calculate local space bounding volume (Transform from root space to SkinnedMeshRenderer space)
		// When we have scaled objects we have to
		// The m_TransformInfo.localAABB is the local space bounding volume relative to the root position and rotation, excluding scale.
		// Thus the following code simply exists to add scale to the stored m_AABB.
		Matrix4x4f inverseWorldMatrix;
		inverseWorldMatrix.SetTRInverse (pos, rot);

		Matrix4x4f rootToLocal;
		MultiplyMatrices4x4(&inverseWorldMatrix, &rootToWorldMatrix, &rootToLocal);

		TransformAABB (localAABB, rootToLocal, m_TransformInfo.localAABB);
	}
}


void SkinnedMeshRenderer::GetSkinnedMeshLocalAABB (AABB& bounds)
{
	// Make sure the localAABB & m_LocalAABB is up to date
	const TransformInfo& info = GetTransformInfo();

	// Extract from world space bounding volume
	if (m_UpdateWhenOffscreen)
		bounds = info.localAABB;
	// Return precomputed local bounding volume
	else
		bounds = m_AABB;

}

void SkinnedMeshRenderer::BakeMesh (Mesh& mesh)
{
	if (m_CachedMesh == NULL)
		return;

	// Making the mesh relative to the root bone is very unintutive when using it as a SkinnedMesh.
	// So don't do that when exporting the mesh.
	PPtr<Transform> oldRootBone = m_RootBone;
	m_RootBone = NULL;

	SkinMeshInfo skin;
#if UNITY_PS3
	if (false == m_CachedMesh->m_Partitions.empty())
	{
		WarningString(Format("Optimized skinned meshes cannot be baked (%s).", m_CachedMesh->GetName()));
		return;
	}

	skin.vertexData = &m_CachedMesh->GetVertexData();
#endif

	if (PrepareSkinCommon(m_CachedMesh->GetAvailableChannels(), SF_NoUpdateVBO, skin))
	{
		mesh.WaitOnRenderThreadUse();

		mesh.SetBoneWeights(NULL, 0);

		const VertexData& skinVertexData = m_CachedMesh->GetVertexData();
		const VertexStreamsLayout& skinStreamsLayout = skinVertexData.GetStreamsLayout();
		const VertexChannelsLayout& skinChannelsLayout = skinVertexData.GetChannelsLayout();
		mesh.ResizeVertices (skin.vertexCount, m_ChannelsInVBO, skinStreamsLayout, skinChannelsLayout);

		skin.outVertices = mesh.GetVertexDataPointer();

		DeformSkinnedMesh(skin);
		skin.Release();

		// Skinning only updates vertex data in "hot" stream (zero). Have to copy
		// all the other data like UVs.
		const UInt32 channelsToCopy = m_ChannelsInVBO & (~skinStreamsLayout.channelMasks[0]);
		CopyVertexDataChannels (skin.vertexCount, channelsToCopy, skinVertexData, mesh.GetVertexData());

		mesh.GetIndexBuffer() = m_CachedMesh->GetIndexBuffer();
		mesh.GetSubMeshes() = m_CachedMesh->GetSubMeshes();
		mesh.SetVertexColorsSwizzled(m_CachedMesh->GetVertexColorsSwizzled());

		mesh.SetChannelsDirty(true, true);

		// Calculate bounding volume
		Matrix4x4f rootPose = GetActualRootBone().GetWorldToLocalMatrixNoScale ();
		MinMaxAABB accurateMinMax;
		Matrix4x4f* poses;
		int poseCount = GetBindposeCount();
		ALLOC_TEMP(poses, Matrix4x4f, poseCount);
		if (CalculateAnimatedPosesWithRoot(rootPose, poses, poseCount) && CalculateBoneBasedBounds(poses, poseCount, accurateMinMax))
			mesh.SetLocalAABB(accurateMinMax);
	}

	m_RootBone = oldRootBone;
}

////@TODO: Write integration test for this!
#if UNITY_EDITOR || SUPPORT_REPRODUCE_LOG

void SkinnedMeshRenderer::HandleOldSkinnedFilter ()
{
	/// Backwards compatibility for old school skinned mesh filters
	/// - Remove mesh renderer
	/// - copy over materials from renderer
	Renderer* meshRenderer = NULL;
	if (GetGameObjectPtr())
		meshRenderer = QueryComponent(MeshRenderer);

	if (meshRenderer)
	{
		#if UNITY_EDITOR
		GetCleanupManager ().MarkForDeletion (meshRenderer, "Obsolete");
		#endif
		SetMaterialArray(meshRenderer->GetMaterialArray(), meshRenderer->GetSubsetIndices());
	}
}

#endif


#if ENABLE_PROFILER
int SkinnedMeshRenderer::GetVisibleSkinnedMeshRendererCount ()
{
	return gActiveSkinnedMeshes.size_slow();
}
#endif

size_t SkinnedMeshRenderer::GetValidBlendShapeWeightCount () const
{
	size_t size = std::min<UInt32>(m_CachedBlendShapeCount, m_BlendShapeWeights.size());
	
	for (int i=size-1;i >= 0;i--)
	{
		if (HasValidWeight(m_BlendShapeWeights[i]))
			return i + 1;
	}	
	return 0;
}

float SkinnedMeshRenderer::GetBlendShapeWeight(UInt32 index) const
{
	size_t size = std::min<UInt32>(m_CachedBlendShapeCount, m_BlendShapeWeights.size());
	if (index >= size)
		return 0.0F;
	else
		return m_BlendShapeWeights[index];
}

void SkinnedMeshRenderer::SetBlendShapeWeight(UInt32 index, float weight)
{
	if (index >= m_CachedBlendShapeCount)
	{
		ErrorStringMsg("Array index (%d) is out of bounds (size=%d)", (int)index, (int)m_BlendShapeWeights.size());
		return;
	}
	
	// The m_BlendShapeWeights array is stored seperately from the mesh.
	// Thus it can go out of sync. 
	// It is bad to fix dependent serialized data in Awake (eg. SkinnedMeshRenderer in scenes could change based on changes in an asset)
	// Thus we resize to the correct size when the user sets a blendshape weight.
	// We can never assume that m_BlendShapeWeights.size() already matches the amount of blendshapes in the used mesh when this function is called.
	if (index >= m_BlendShapeWeights.size())
		m_BlendShapeWeights.resize_initialized(m_CachedBlendShapeCount, 0.0F);
	
	m_BlendShapeWeights[index] = weight;
	SetDirty();
}

/*
 *  Make sure m_BlendShapeWeights is not accessed directly anywhere.. It is not the size of the number of blendshapes..
 *  Wrong: 	bool ShouldRecalculateBoundingVolumeEveryFrame () { return m_UpdateWhenOffscreen || m_RootBone.GetInstanceID() != 0 || !m_BlendShapeWeights.empty (); }
 *  Wrong: 	BlendShape bounding volume calculation needs serious review
 *  Extract & Store blendshape default values in skinned mesh renderer from fbx file
 *  Review code for multithreaded rendering integration when changing blendshape weights. Probably should also hook into mesh changed for recalculating blendshape count...
 *  Fix animation window editing being abysmal slow
*/
void SkinnedMeshRenderer::UnloadVBOFromGfxDevice()
{
	if (m_VBO)
	{
		GetGfxDevice().DeleteVBO (m_VBO);
		m_VBO = NULL;
	}
	if (m_MemExportInfo)
	{
		GetGfxDevice().DeleteGPUSkinningInfo(m_MemExportInfo);
		m_MemExportInfo = NULL;
	}

	m_SourceMeshDirty = true;
}

void SkinnedMeshRenderer::ReloadVBOToGfxDevice()
{
	if (!m_MemExportInfo)
		m_MemExportInfo = CreateGPUSkinningIfAvailable();
}

Unity::Component* SkinnedMeshRenderer::GetAnimator()
{
	if (m_CachedAnimator == NULL)
		CreateCachedAnimatorBinding ();

	return m_CachedAnimator;
}

void SkinnedMeshRenderer::CreateCachedAnimatorBinding()
{
	ClearCachedAnimatorBinding();

	if (!m_CachedMesh)
		return;

	const dynamic_array<BindingHash>& bonePathHashes = m_CachedMesh->GetBonePathHashes();
	if (bonePathHashes.size() != GetBindposeCount())
	{
		ErrorStringObject("Bones do not match bindpose.", m_CachedMesh);
		return;
	}

	m_CachedAnimator = FindAncestorComponentExactTypeImpl(GetGameObject(), ClassID(Animator));
	if (m_CachedAnimator == NULL || GetAnimationInterface() == NULL)
		return;

	m_SkeletonIndices.resize_uninitialized(bonePathHashes.size());
	if (!GetAnimationInterface()->PathHashesToIndices(*m_CachedAnimator, bonePathHashes.begin(), bonePathHashes.size(), m_SkeletonIndices.begin()))
		m_SkeletonIndices.clear();

	Assert(!m_CachedAnimator->HasEvent(AnimatorModifiedCallback, this));
	m_CachedAnimator->AddEvent(AnimatorModifiedCallback, this);
}

void SkinnedMeshRenderer::ClearCachedAnimatorBinding()
{
	if (m_CachedAnimator == NULL)
		return;

	m_CachedAnimator->RemoveEvent(AnimatorModifiedCallback, this);
	m_SkeletonIndices.clear();
	m_CachedAnimator = NULL;
}

void SkinnedMeshRenderer::AnimatorModifiedCallback(void* userData, void* sender, int eventID)
{
	SkinnedMeshRenderer& skinnedMeshRenderer = *reinterpret_cast<SkinnedMeshRenderer*>(userData);

	if (eventID == kAnimatorClearEvent)
		skinnedMeshRenderer.ClearCachedAnimatorBinding();
}

