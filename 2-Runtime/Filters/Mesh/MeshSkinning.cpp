#include "UnityPrefix.h"
#include "MeshSkinning.h"
#if UNITY_OSX
#include <alloca.h> // this is really deprecated and should be exchanged for stdlib.h
#else
#include <stdlib.h>
#endif
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/OptimizationUtility.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Utilities/Prefetch.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Misc/CPUInfo.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Filters/Mesh/LodMesh.h"

PROFILER_INFORMATION(gMeshSkinningProfile, "MeshSkinning.Skin", kProfilerRender)
PROFILER_INFORMATION(gMeshSkinningSlowpath, "MeshSkinning.SlowPath", kProfilerRender)

#include "MeshSkinningMobile.h"
#include "MeshSkinningSSE2.h"
#include "SkinGeneric.h"
#include "MeshBlendShaping.h"


//===========================================================================================================================================


void SkinMesh(SkinMeshInfo& info)
{
	const TransformInstruction NormalizeTransformInstruction =
#if (UNITY_SUPPORTS_NEON && !UNITY_DISABLE_NEON_SKINNING) || UNITY_SUPPORTS_VFP
		// NOTE: optimized NEON/VFP routines do not do any normalization
		// instead we rely on GPU to do that
		kNoNormalize;
#else
		//@TODO: fix that "Fast" & "Fastest" crap. Right now "Fastest" is actually a win on PC (1ms saved in Dark Unity)
		// so I'm leaving it there for now.
		kNormalizeFastest;
#endif
	
	// Instantiates the right skinning template depending on the bone per vertex count
	#define PERMUTE_BONES(skinNormal,skinTangent) { \
	if (info.bonesPerVertex == 1) \
		SkinGeneric<NormalizeTransformInstruction, 1, skinNormal, skinTangent> (info); \
	else if (info.bonesPerVertex == 2) \
		SkinGeneric<NormalizeTransformInstruction, 2, skinNormal, skinTangent> (info); \
	else if (info.bonesPerVertex == 4) \
		SkinGeneric<NormalizeTransformInstruction, 4, skinNormal, skinTangent> (info); \
	}

	if (info.skinNormals && info.skinTangents)
		PERMUTE_BONES(true, true)
	else if (info.skinNormals)
		PERMUTE_BONES(true, false)
	else
		PERMUTE_BONES(false, false)
}


static void ApplyMeshSkinning (SkinMeshInfo& info)
{
	#if UNITY_WII
	SkinMeshWii(info);
	#else
	
	PROFILER_AUTO(gMeshSkinningProfile, NULL);

	if (SkinMeshOptimizedMobile(info))
		return;

	if (SkinMeshOptimizedSSE2(info))
		return;
	
	// fallback to slow generic implementation
	{
		PROFILER_AUTO(gMeshSkinningSlowpath, NULL);
		SkinMesh(info);
	}
	#endif	
}

void DeformSkinnedMesh (SkinMeshInfo& info)
{
	const bool hasBlendShapes = info.blendshapeCount != 0;
	const bool hasSkin = info.boneCount != 0;

	// No actual skinning can be done. Just copy vertex stream.
	// TODO: This code can be removed if we render the undeformed mesh in SkinnedMeshRenderer
	// when there is no skin and no active blend shapes. See case 557165.
	if (!hasBlendShapes && !hasSkin)
	{
		memcpy (info.outVertices, info.inVertices, info.inStride * info.vertexCount);
		return;
	}

	UInt8* tmpBlendShapes = NULL;

	// blend shapes
	if (hasBlendShapes)
	{
		// The final destination might be write-combined memory which is insanely slow to read
		// or randomly access, so always allocate a temp buffer for blend shapes (case 554830).
		// Skinning can write directly to a VB since it always writes sequentially to memory.
		size_t bufferSize = info.inStride * info.vertexCount;
		tmpBlendShapes = ALLOC_TEMP_MANUAL(UInt8, bufferSize);
		
		ApplyBlendShapes (info, tmpBlendShapes);
		
		if (hasSkin)
			info.inVertices = tmpBlendShapes;
		else
			memcpy(info.outVertices, tmpBlendShapes, bufferSize);
	}

	// skinning
	if (hasSkin)
		ApplyMeshSkinning (info);

	if (tmpBlendShapes)
		FREE_TEMP_MANUAL(tmpBlendShapes);
}


void* DeformSkinnedMeshJob (void* rawData)
{
	SkinMeshInfo* data = reinterpret_cast<SkinMeshInfo*>(rawData);
	DeformSkinnedMesh (*data);
	return NULL;
}


SkinMeshInfo::SkinMeshInfo()
{
	memset(this, 0, sizeof(SkinMeshInfo));
}

void SkinMeshInfo::Allocate()
{
	size_t size = boneCount * sizeof(Matrix4x4f) + sizeof(float) * blendshapeCount;
	if (size == 0)
		return;
	
	allocatedBuffer = (UInt8*)UNITY_MALLOC_ALIGNED(kMemSkinning, size, 64);
	
	UInt8* head = allocatedBuffer;
	if (boneCount != 0)
	{
		cachedPose = reinterpret_cast<Matrix4x4f*> (head);
		head += sizeof(Matrix4x4f)  * boneCount;
	}
	
	if (blendshapeCount != 0)
{
		blendshapeWeights = reinterpret_cast<float*> (head);
	}
}

void SkinMeshInfo::Release() const
{
	if (allocatedBuffer)
		UNITY_FREE(kMemSkinning, allocatedBuffer);
}
