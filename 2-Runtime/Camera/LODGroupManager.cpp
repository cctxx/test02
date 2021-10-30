#include "UnityPrefix.h"
#include "LODGroupManager.h"
#include "LODGroup.h"
#include "CullingParameters.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Camera/RenderLoops/BuiltinShaderParamUtility.h"
#include "Runtime/Misc/BuildSettings.h"

PROFILER_INFORMATION (gComputeLOD, "LOD.ComputeLOD", kProfilerRender)

LODGroupManager* gLODGroupManager = NULL;

LODGroupManager& GetLODGroupManager ()
{
	Assert(gLODGroupManager != NULL);
	return *gLODGroupManager;
}

LODGroupManager* GetLODGroupManagerPtr ()
{
	return gLODGroupManager;
}
void CleanupLODGroupManager ()
{
	Assert(gLODGroupManager != NULL);
	UNITY_DELETE (gLODGroupManager, kMemRenderer);
}

void InitializeLODGroupManager ()
{
	Assert(gLODGroupManager == NULL);
	gLODGroupManager = UNITY_NEW_AS_ROOT(LODGroupManager(), kMemRenderer, "LODGroupManager", "");
}


LODGroupManager::LODGroupManager ()
{
	m_LODBias = 1.0F;
	m_MaximumLOD = 0;
	
	memset(&m_SelectionData.push_back(), 0, sizeof(LODSelectionData));
}

// The basic LOD distance check in orthomode:
// * Pixel size check: if (array[i].pixelHeight < lodGroup.m_Size * 0.5F / parameters.orthoSize * parameters.cameraPixelHeight)
// * Relative height check: if (array[i].relativeHeight < lodGroup.m_Size * 0.5F / parameters.orthoSize)

// The basic LOD distance check in perspective:
// ...

// All LOD calculations are distance based "reference point in LOD group" to camera position.
// - Rotating a camera never switches LOD
// - Point based means it's very predictable behaviour that is easy to visualize accurately
// - Fast to calculate
//  float distance = CalculateFOVDistanceFudge () * CaclulateLODDistance();


float CalculateFOVHalfAngle (const CullingParameters& parameters)
{
	return tan(Deg2Rad (parameters.lodFieldOfView) * 0.5F);
}

enum { kScreenRelativeMetric = 0, kPixelMetric = 1, kMetricCount = 2 };

void CalculateLODFudge (const CullingParameters& parameters, float* fudge)
{
	float screenRelativeMetric;
	if (parameters.isOrthographic)
	{
		screenRelativeMetric = 2.0F * parameters.orthoSize;
	}
	else
	{
		// Half angle at 90 degrees is 1.0 (So we skip halfAngle / 1.0 calculation)
		float halfAngle = CalculateFOVHalfAngle(parameters);
		screenRelativeMetric = 2.0 * halfAngle;
	}

	fudge[kScreenRelativeMetric] = screenRelativeMetric;
	fudge[kPixelMetric] = screenRelativeMetric / parameters.cameraPixelHeight;
}

float CalculateLODDistance (float relativeScreenHeight, float size)
{
	return size / relativeScreenHeight;
}

float DistanceToRelativeHeight (const CullingParameters& parameters, float distance, float size)
{
	if (parameters.isOrthographic)
	{
		return size * 0.5F / parameters.orthoSize;
	}
	else
	{
		float halfAngle = CalculateFOVHalfAngle(parameters);
		return size * 0.5F / (distance * halfAngle);
	}
}


void LODGroupManager::CalculatePerspectiveLODMask (const LODSelectionData& selection, const Vector3f& position, int maximumLOD, int currentMask, const float* fieldOfViewFudge, UInt8* output, float* fade)
{
	if (selection.forceLODLevelMask != 0)
	{
		*output = selection.forceLODLevelMask;
		*fade = 1.0F;
		return;
	}
	
	Vector3f offset = selection.worldReferencePoint - position;
	
	float sqrDistance = SqrMagnitude(offset);
	sqrDistance *= fieldOfViewFudge[kScreenRelativeMetric] * fieldOfViewFudge[kScreenRelativeMetric];
	
	// Early out if the object is getting culled because it is too far away.
	*output = 0;
	*fade = 0.0F;
	
	// Must use the same metric for everything.... Otherwise this will fail
	if (sqrDistance > selection.maxDistanceSqr)
		return;
	
	int maxDistancesCount = selection.maxDistancesCount;
	const float* distances = selection.maxDistances;
	
	bool supportsFade = selection.fadeDistance != 0.0F;
	
	for (int i=maximumLOD;i<maxDistancesCount;i++)
	{
		// Is camera closer than maximum LOD distance?
		float lodMaxDistance = distances[i];
		float lodMaxDistanceSqr = lodMaxDistance * lodMaxDistance;
		
		if (sqrDistance < lodMaxDistanceSqr)
		{
			// @TODO: This if could be optimized out of the inner loop
			if (supportsFade)
			{
				// Is the next LOD in the transition range?
				float dif = lodMaxDistance - sqrt(sqrDistance);
				if (dif < selection.fadeDistance)
				{
					currentMask |= currentMask << 1;
					*output = currentMask;
					*fade = dif / selection.fadeDistance;
				}
				else
				{
					///@TODO: this should be zero, because when you are not fading it shouldn't use a shader that does fading.
					*output = currentMask;
					*fade = 1.0F;
				}
			}
			else
			{
				///@TODO: this should be zero, because when you are not fading it shouldn't use a shader that does fading.
				*output = currentMask;
				*fade = 1.0F;
			}
			
			return;
		}
		
		currentMask <<= 1;
	}
}

void LODGroupManager::CalculateOrthoLODMask (const LODSelectionData& selection, int maximumLOD, int currentMask, const float* fudge, UInt8* output, float* fade)
{
	if (selection.forceLODLevelMask != 0)
	{
		*output = selection.forceLODLevelMask;
		*fade = 1.0F;
		return;
	}
	
	///@TODO: DO IT
	*output = 0;
	*fade = 0.0F;
	
	int maxDistancesCount = selection.maxDistancesCount;
	const float* distances = selection.maxDistances;

	float distance = fudge[kScreenRelativeMetric];

	for (int i=maximumLOD;i<maxDistancesCount;i++)
	{
		if (distance < distances[i])
		{
			// Is the next LOD in the transition range?
			float dif = distances[i] - distance;
			if (dif < selection.fadeDistance)
			{
				currentMask |= currentMask << 1;
				*output = currentMask;
				*fade = dif / selection.fadeDistance;
			}
			else
			{
				///@TODO: this should be zero, because when you are not fading it shouldn't use a shader that does fading.
				*output = currentMask;
				*fade = 1.0F;
			}
			
			return;
		}
		
		currentMask <<= 1;
	}
}

void LODGroupManager::CalculateLODMasks (const CullingParameters& parameters, UInt8* outMasks, float* outFades)
{
	PROFILER_AUTO(gComputeLOD, NULL)

	// Get field of view / pixel fudge values and sqr it so the inner loop doesn't have to do it.
	float fieldOfViewFudge[kMetricCount];
	CalculateLODFudge (parameters, fieldOfViewFudge);
	for (int i=0;i<kMetricCount;i++)
		fieldOfViewFudge[i] = fieldOfViewFudge[i] / m_LODBias;

	int lodGroupCount = m_SelectionData.size();
	DebugAssert(lodGroupCount > 0);
	outMasks[0] = 0;
	outFades[0] = 0;
	int baseMask = 1 << m_MaximumLOD;
	if (parameters.isOrthographic)
	{
		for (int i=1;i<lodGroupCount;i++)
			CalculateOrthoLODMask(m_SelectionData[i], m_MaximumLOD, baseMask, fieldOfViewFudge, &outMasks[i], &outFades[i]);
	}
	else
	{
		for (int i=1;i<lodGroupCount;i++)
			CalculatePerspectiveLODMask(m_SelectionData[i], parameters.lodPosition, m_MaximumLOD, baseMask, fieldOfViewFudge, &outMasks[i], &outFades[i]);
	}
}

inline UInt32 LowestBit2Consecutive8Bit (UInt32 v)
{
	Assert(v == (v & 0xff));
	UInt32 extra = (v & (v - 1)) == 0;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v++;
	
	UInt32 table[] = { v >> 2, v >> 1 };
	return table[extra];
}

// The shader isn't using clamp on the z value of the 3D dither texture.
// Thus we have to ensure that it never actually becomes 1.0
inline float ClampForGPURepeat (float fade)
{
	return clamp(fade, 0.0F, LOD_FADE_DISABLED);
}

float LODGroupManager::CalculateLODFade (UInt32 lodGroupIndex, UInt32 rendererActiveLODMask, const UInt8* lodMasks, const float* lodFades)
{
	if (rendererActiveLODMask == 0)
		return LOD_FADE_DISABLED;
	
	// rendererActiveLODMask:
	// The mask of all LOD levels that this renderer participates in.
	
	// The mask of all active LOD levels for this group.
	// Eg. LOD 0 and LOD1 enabled -> 1 | 2
	UInt8 activeMaskOfLODGroup = lodMasks[lodGroupIndex];
	
	// If renderer is part of all active LOD groups, then it should be completely visible.
	if ((rendererActiveLODMask & activeMaskOfLODGroup) == activeMaskOfLODGroup)
		return LOD_FADE_DISABLED;
	
	// If the renderer is part of the lowest bit then it is part of the highest LOD (duh!)
	bool isPartOfHighLOD = LowestBit2Consecutive8Bit (activeMaskOfLODGroup) & rendererActiveLODMask;
	
	// The highest LOD Level uses the computed fade value
	if (isPartOfHighLOD)
	{
		return ClampForGPURepeat(lodFades[lodGroupIndex]);
	}
	// The lower lod level uses the inverse
	else
	{
		return ClampForGPURepeat(1.0F - lodFades[lodGroupIndex]);
	}
}

void LODGroupManager::ClearAllForceLODMask ()
{
	for (int i=0;i<m_SelectionData.size();i++)
		m_SelectionData[i].forceLODLevelMask = 0;

}

#if UNITY_EDITOR
static void AddRenderersToVisualizationStats (const LODGroup::LODRenderers& renderers, LODVisualizationInformation& information)
{
	// Calculate triangle & vertex & mesh count for the renderers in this LOD
	for (int i=0;i<renderers.size();i++)
	{
		Renderer* renderer = renderers[i].renderer;
		if (renderer)
		{
			RenderStats stats;
			renderer->GetRenderStats (stats);
			
			information.triangleCount += stats.triangleCount;
			information.vertexCount   += stats.vertexCount;
			information.rendererCount += 1;
			information.submeshCount  += stats.submeshCount;
		}
	}
}

LODVisualizationInformation LODGroupManager::CalculateVisualizationData (const CullingParameters& cullingParameters, LODGroup& lodGroup, int lodLevel)
{
	LODVisualizationInformation information;
	memset(&information, 0, sizeof(information));
	information.activeLODLevel = kInvalidLODGroup;
	
	// Calculate switch distance
	float fudge[kMetricCount];
	CalculateLODFudge (cullingParameters, fudge);
	for (int i=0;i<kMetricCount;i++)
		fudge[i] = fudge[i] / m_LODBias;
	
	float sqrFudge[kMetricCount];
	for (int i=0;i<kMetricCount;i++)
		sqrFudge[i] = fudge[i];
	float lodFade = 0.0F;

	if (lodGroup.m_LODGroup != kInvalidLODGroup && lodGroup.m_LODGroup != kDisabledLODGroup)
	{
		UInt8 activeLODMask;
		if (cullingParameters.isOrthographic)
			CalculateOrthoLODMask (m_SelectionData[lodGroup.m_LODGroup], m_MaximumLOD, 1 << m_MaximumLOD, sqrFudge, &activeLODMask, &lodFade);
		else
			CalculatePerspectiveLODMask (m_SelectionData[lodGroup.m_LODGroup], cullingParameters.lodPosition, m_MaximumLOD, 1 << m_MaximumLOD, sqrFudge, &activeLODMask, &lodFade);
		
		if (activeLODMask != 0)
			information.activeLODLevel = LowestBit(activeLODMask);
		else
			information.activeLODLevel = -1;
	}
	
	if (lodLevel == -1)
		lodLevel = information.activeLODLevel;

	// Calculate current distances & bounding volume sizes
	information.activeDistance = Magnitude(lodGroup.GetWorldReferencePoint() - cullingParameters.lodPosition);
	information.activeRelativeScreenSize = DistanceToRelativeHeight(cullingParameters, information.activeDistance, lodGroup.GetWorldSpaceSize()) * m_LODBias;
	information.activePixelSize = information.activeRelativeScreenSize * cullingParameters.cameraPixelHeight;
	information.activeWorldSpaceSize = lodGroup.GetWorldSpaceSize ();
	information.activeLODFade = lodFade;
	
	if (lodLevel != -1)
	{
		// Calculate switch distance for the lod
		const LODGroup::LOD& lod = lodGroup.m_LODs[lodLevel];
		
		// Calculate triangle & vertex & mesh count for the renderers in this LOD
		AddRenderersToVisualizationStats(lod.renderers, information);
	}
	
	return information;
}
#endif //UNITY_EDITOR

void LODGroupManager::AddLODGroup (LODGroup& group, const Vector3f& position, float worldSpaceSize)
{
	// Add Group
	int index = m_SelectionData.size();
	m_SelectionData.push_back();
	group.m_LODGroup = index;

	// Initialize parameters
	UpdateLODGroupParameters(index, group, position, worldSpaceSize);
	
	m_SelectionData.back().forceLODLevelMask = 0;
}

void  LODGroupManager::UpdateLODGroupParameters (int index, LODGroup& group, const Vector3f& position, float worldSpaceSize)
{
	LODSelectionData& data = m_SelectionData[index];

	data.worldReferencePoint = position;
	data.lodGroup = &group;
	data.maxDistancesCount = group.m_LODs.size();
	data.maxDistanceSqr = 0.0F;
	
	Assert(group.m_LODs.size() <= kMaximumLODLevels);
	
	float totalMaxDistance = 0.0F;
	
	if (!GetBuildSettings ().hasAdvancedVersion)
	{
		data.maxDistancesCount = std::max (1, data.maxDistancesCount);
		totalMaxDistance = data.maxDistances[0] = CalculateLODDistance(0.0001F, worldSpaceSize);
	}
	else
	{
		
		for (int i=0;i<group.m_LODs.size();i++)
		{
			float maxDistance =	CalculateLODDistance(group.m_LODs[i].screenRelativeHeight, worldSpaceSize);
		
			totalMaxDistance = std::max(maxDistance, totalMaxDistance);
			data.maxDistances[i] = maxDistance;
		}
	}

	data.maxDistanceSqr = totalMaxDistance * totalMaxDistance;
	
	bool useLODFade = group.m_ScreenRelativeTransitionHeight > 0.00001F && !group.m_LODs.empty();
	if (useLODFade)
	{
		float baseRelativeHeight = group.m_LODs.front().screenRelativeHeight;
		data.fadeDistance = CalculateLODDistance (baseRelativeHeight - group.m_ScreenRelativeTransitionHeight, worldSpaceSize) - CalculateLODDistance (baseRelativeHeight, worldSpaceSize);
	}
	else
		data.fadeDistance = 0.0F;
}

void LODGroupManager::RemoveLODGroup (LODGroup& group)
{
	// Remove from array by replacing with the last element.
	Assert(group.m_CachedRenderers.empty());
	
	int index = group.m_LODGroup;
	Assert(index != 0);
	
	// Update LODGroup index of the LODGroup we moved from the back
	m_SelectionData.back().lodGroup->NotifyLODGroupManagerIndexChange(index);

	m_SelectionData[index] = m_SelectionData.back();
	m_SelectionData.pop_back();
	
	group.m_LODGroup = kInvalidLODGroup;
}


#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (LODGroupManagerTests)
{
TEST (LODGroupManagerTests_PrevPowerOfTwoUInt8)
{
	for (int i=0;i<7;i++)
	{
		CHECK_EQUAL (1 << i, LowestBit2Consecutive8Bit(1 << i));
		CHECK_EQUAL (1 << i, LowestBit2Consecutive8Bit((1 << i) | (1 << (i+1))));
	}
}
}

#endif
