#include "UnityPrefix.h"
#include "SceneCulling.h"
#include "CullingParameters.h"
#include "Runtime/Geometry/Intersection.h"
#include "UmbraBackwardsCompatibility.h"
#include "SceneNode.h"
#include "Runtime/Geometry/AABB.h"


using Umbra::OcclusionBuffer;

static bool IsLayerDistanceCulled (UInt32 layer, const AABB& aabb, const SceneCullingParameters& params);
static void ExtractUmbraCameraTransformFromCullingParameters (const CullingParameters& params, Umbra::CameraTransform& output);
static void CullSceneWithLegacyUmbraDeprecated (SceneCullingParameters& cullingParams, CullingOutput& output, Umbra::Query* q, const Umbra_3_0::Tome* t);
	
// Reduce the index list using Scene::IsNodeVisible.
// Takes the index list as input and output, after the function the indices that are not visible will have been removed.
static void ProcessIndexListIsNodeVisible (IndexList& list, const SceneNode* nodes, const AABB* bounds, const SceneCullingParameters& params)
{
	int size = list.size;
	// Check layers and cull distances
	int visibleCountNodes = 0;
	for (int i = 0; i < size; ++i)
	{
		int index = list.indices[i];
		const SceneNode& node = nodes[index];
		const AABB& aabb = bounds[index];
		if (IsNodeVisible(node, aabb, params))
			list.indices[visibleCountNodes++] = index;
	}
	
	list.size = visibleCountNodes;
}


static void CullDynamicObjectsWithoutUmbra (IndexList& visibleObjects, const SceneCullingParameters& cullingParams, const AABB* aabbs, size_t count)
{
	int visibleIndex = 0;
	for (int i=0;i<count;i++)
	{
		const AABB& bounds = aabbs[i];
		if (IntersectAABBPlaneBounds(bounds, cullingParams.cullingPlanes, cullingParams.cullingPlaneCount))
			visibleObjects[visibleIndex++] = i;
	}
	
	visibleObjects.size = visibleIndex;
}

static void CullDynamicObjectsUmbra (IndexList& visibleObjects, const AABB* aabbs, size_t count, OcclusionBuffer* occlusionBuffer)
{
    // TODO dynamic objects with legacy Tomes properly
	int visibleIndex = 0;
	for (int i=0;i<count;i++)
	{
		const AABB& bounds = aabbs[i];
		
		Vector3f mn = bounds.GetMin();
		Vector3f mx = bounds.GetMax();
		Umbra::OcclusionBuffer::VisibilityTestResult result = occlusionBuffer->testAABBVisibility((Umbra::Vector3&)mn, (Umbra::Vector3&)mx);
		
		if (result != OcclusionBuffer::OCCLUDED)
			visibleObjects[visibleIndex++] = i;
	}
	
	visibleObjects.size = visibleIndex;
}

void CullSceneWithoutUmbra (const SceneCullingParameters& cullingParams, CullingOutput& output)
{
	for (int i=0;i<kVisibleListCount;i++)
	{
		CullDynamicObjectsWithoutUmbra (output.visible[i], cullingParams, cullingParams.renderers[i].bounds, cullingParams.renderers[i].rendererCount);
		ProcessIndexListIsNodeVisible  (output.visible[i], cullingParams.renderers[i].nodes, cullingParams.renderers[i].bounds, cullingParams);
	}
}

void CullSceneWithUmbra (SceneCullingParameters& cullingParams, CullingOutput& output)
{
#if SUPPORT_BACKWARDS_COMPATIBLE_UMBRA
	if (cullingParams.umbraTome.IsLegacyTome())
    {
        CullSceneWithLegacyUmbraDeprecated(cullingParams, output, cullingParams.umbraQuery, cullingParams.umbraTome.legacyTome);
		return;
    }
#endif
	
	DebugAssert(cullingParams.useOcclusionCulling);
	
	const Umbra::Tome* tome = cullingParams.umbraTome.tome;
	Umbra::QueryExt* query = cullingParams.umbraQuery;
	
	Assert(tome != NULL);
	
	Umbra::CameraTransform camera;
	ExtractUmbraCameraTransformFromCullingParameters(cullingParams, camera);
	
	Umbra::Query::ErrorCode e;
	
	Umbra::UINT32 umbraFlags = cullingParams.umbraDebugFlags;
	query->setDebugRenderer(cullingParams.umbraDebugRenderer);

    // Legacy Umbra queries for PVS Tomes
	Assert(tome->getStatistic(Umbra::Tome::STAT_PORTAL_DATA_SIZE) != 0);
	e = query->queryPortalVisibility(umbraFlags, *output.umbraVisibility, camera);

	//If camera is out of generated view volumes (case 554981), fall back to view frustum culling
	if (e == Umbra::Query::ERROR_OUTSIDE_SCENE)
	{
		cullingParams.useOcclusionCulling = false;
		cullingParams.useShadowCasterCulling = false;
		CullSceneWithoutUmbra( cullingParams, output );
		return;
	}

	// Process static objects with Unity specific culling (cullingMask / LOD etc)
	IndexList& staticObjects = output.visible[kStaticRenderers];
	staticObjects.size = output.umbraVisibility->getOutputObjects()->getSize();
	
	ProcessIndexListIsNodeVisible(output.visible[kStaticRenderers], cullingParams.renderers[kStaticRenderers].nodes, cullingParams.renderers[kStaticRenderers].bounds, cullingParams);
	output.umbraVisibility->getOutputObjects()->setSize(staticObjects.size);
	
	// Process dynamic objects
	for (int i=kDynamicRenderer;i<kVisibleListCount;i++)
	{
		CullDynamicObjectsUmbra      (output.visible[i], cullingParams.renderers[i].bounds, cullingParams.renderers[i].rendererCount, output.umbraVisibility->getOutputBuffer());
		ProcessIndexListIsNodeVisible(output.visible[i], cullingParams.renderers[i].nodes, cullingParams.renderers[i].bounds, cullingParams);
	}
}

static void SplitCombinedDynamicList (Umbra::IndexList& indices, CullingOutput& sceneCullingOutput)
{
	int startIndices[kVisibleListCount];
	startIndices[kStaticRenderers] = -1;
	int offset = 0;
	for (int t=kDynamicRenderer;t<kVisibleListCount;t++)
	{
		startIndices[t] = offset;
		offset += sceneCullingOutput.visible[t].reservedSize;
	}
	
	int *indexPtr = indices.getPtr();
	for (int i=0;i<indices.getSize();i++)
	{
		int index = indexPtr[i];

		for (int t=kVisibleListCount-1; t>= 0 ;t--)
		{
			DebugAssert(t != kStaticRenderers);
			if (index >= startIndices[t])
			{
				IndexList& indices = sceneCullingOutput.visible[t];
				indices[indices.size++] = index - startIndices[t];
				break;
			}
		}
	}
}

// Unity has multiple dynamic bounding volume arrays.
// Umbra's shadow caster culling needs a single list of all dynamic renderers
static void GenerateCombinedDynamicList (const CullingOutput& sceneCullingOutput, const RendererCullData* renderers, dynamic_array<int>& indexList, dynamic_array<Vector3f>& minMaxBounds)
{
	size_t visibleSize = 0;
	size_t totalSize = 0;
	for (int t=kDynamicRenderer;t<kVisibleListCount;t++)
	{
		visibleSize += sceneCullingOutput.visible[t].size;
		totalSize += sceneCullingOutput.visible[t].reservedSize;
	}
	
	indexList.resize_uninitialized(visibleSize);
	minMaxBounds.resize_uninitialized(totalSize * 2);
	
	// Make one big index list indexing into the combined bounding volume array
	int index = 0;
	int baseOffset = 0;
	for (int t=kDynamicRenderer;t<kVisibleListCount;t++)
	{
		int* visibleDynamic = sceneCullingOutput.visible[t].indices;
		for (int i=0;i<sceneCullingOutput.visible[t].size;i++)
			indexList[index++] = visibleDynamic[i] + baseOffset;
		
		baseOffset += sceneCullingOutput.visible[t].reservedSize;
	}

	// Create one big bounding volume array
	index = 0;
	for (int t=kDynamicRenderer;t<kVisibleListCount;t++)
	{
		const AABB* aabbs = renderers[t].bounds;
		
		Assert(renderers[t].rendererCount == sceneCullingOutput.visible[t].reservedSize);
		for (int i=0;i<renderers[t].rendererCount;i++)
		{
			minMaxBounds[index++] = aabbs[i].GetMin();
			minMaxBounds[index++] = aabbs[i].GetMax();
		}
	}
}

void CullShadowCastersWithUmbra (const SceneCullingParameters& cullingParams, CullingOutput& output)
{
	DebugAssert(cullingParams.useShadowCasterCulling && cullingParams.useOcclusionCulling);
	
	Assert( cullingParams.umbraTome.tome != NULL);
	Umbra::QueryExt* query = (Umbra::QueryExt*)cullingParams.umbraQuery;

	Umbra::CameraTransform camera;
	ExtractUmbraCameraTransformFromCullingParameters(cullingParams, camera);

	const Umbra::Visibility* inputSceneVisibility = cullingParams.sceneVisbilityForShadowCulling->umbraVisibility;

	dynamic_array<int> visibleSceneIndexListCombined (kMemTempAlloc);
	dynamic_array<Vector3f> dynamicBounds (kMemTempAlloc);

	////@TODO: This extra copying could maybe be removed or moved to not be per light?
	GenerateCombinedDynamicList (*cullingParams.sceneVisbilityForShadowCulling, cullingParams.renderers, visibleSceneIndexListCombined, dynamicBounds);

	size_t totalDynamicCount = dynamicBounds.size() / 2;
	size_t totalStaticCount = cullingParams.renderers[kStaticRenderers].rendererCount;
	
	int* dynamicCasterIndices;
	ALLOC_TEMP(dynamicCasterIndices, int, totalDynamicCount);
	
	// The output visible static & dynamic caster index lists
	Umbra::IndexList staticCasterList (output.visible[kStaticRenderers].indices, totalStaticCount);
	Umbra::IndexList dynamicCasterList(dynamicCasterIndices, totalDynamicCount);
	
	// The current scene visibility (input for umbra occlusion culling)
	const Umbra::IndexList sceneVisibleDynamicIndexList (visibleSceneIndexListCombined.begin(), visibleSceneIndexListCombined.size(), visibleSceneIndexListCombined.size());

    
	query->setDebugRenderer(cullingParams.umbraDebugRenderer);

    // TODO: this can be done right after the visibility query and only needs to be done once
    // per camera position
    // TODO: store shadowCuller so that it can be shared across threads
    Umbra::ShadowCullerExt* shadowCuller;
    ALLOC_TEMP(shadowCuller, Umbra::ShadowCullerExt, 1);
    new (shadowCuller) Umbra::ShadowCullerExt(
        cullingParams.umbraTome.tome,
        inputSceneVisibility,
        (Umbra::Vector3&)cullingParams.lightDir,
        (const Umbra::Vector3*)dynamicBounds.data(),
		totalDynamicCount,
		&sceneVisibleDynamicIndexList);

    // The last two parameters: jobIdx, numTotalJobs. These can be called from mulitple threads
    query->queryStaticShadowCasters (*shadowCuller, staticCasterList, 0, 1);
    query->queryDynamicShadowCasters(*shadowCuller, dynamicCasterList, (const Umbra::Vector3*)dynamicBounds.data(), totalDynamicCount);

	output.visible[kStaticRenderers].size = staticCasterList.getSize();
	SplitCombinedDynamicList (dynamicCasterList, output);
	
	for (int i=0;i<kVisibleListCount;i++)
		ProcessIndexListIsNodeVisible  (output.visible[i], cullingParams.renderers[i].nodes, cullingParams.renderers[i].bounds, cullingParams);
}


bool IsNodeVisible (const SceneNode& node, const AABB& aabb, const SceneCullingParameters& params)
{
	UInt32 layermask = 1 << node.layer;
	if ((layermask & params.cullingMask) == 0)
		return false;
	
	// Static renderers are not deleted from the renderer objects list.
	// This is because we ensure that static objects come first and the pvxIndex in umbra matches the index in m_RendererNodes array.
	// However the layerMask is set to 0 thus it should be culled at this point
	if (node.renderer == NULL)
		return false;
	
	// Check if node was deleted but not removed from list yet.
	if (node.disable)
		return false;
	
	if (node.lodIndexMask != 0)
	{
		DebugAssert(node.lodGroup < params.lodGroupCount);
		if ((params.lodMasks[node.lodGroup] & node.lodIndexMask) == 0)
			return false;
	}
	
	if (IsLayerDistanceCulled(node.layer, aabb, params))
		return false;
	
	return true;
}


static bool IsLayerDistanceCulled (UInt32 layer, const AABB& aabb, const SceneCullingParameters& params)
{
	switch (params.layerCull)
	{
		case CullingParameters::kLayerCullPlanar:
		{
			Assert(params.cullingPlaneCount == kPlaneFrustumNum);
			Plane farPlane = params.cullingPlanes[kPlaneFrustumFar];
			farPlane.distance = params.layerFarCullDistances[layer];
			return !IntersectAABBPlaneBounds (aabb, &farPlane, 1);
		}
		case CullingParameters::kLayerCullSpherical:
		{
			float cullDist = params.layerFarCullDistances[layer];
			if (cullDist == 0.0f)
				return false;
			return (SqrMagnitude(aabb.GetCenter() - params.position) > Sqr(cullDist));
		}
		default:
			return false;
	}
}
	
void ExtractUmbraCameraTransformFromCullingParameters (const CullingParameters& params, Umbra::CameraTransform& output)
{
	if  (params.cullingPlaneCount > 6)
		output.setUserClipPlanes((Umbra::Vector4*)&params.cullingPlanes[6], params.cullingPlaneCount-6);
	output.set((Umbra::Matrix4x4&)params.worldToClipMatrix, (Umbra::Vector3&)params.position);
}

#if SUPPORT_BACKWARDS_COMPATIBLE_UMBRA

static void CullDynamicObjectsUmbraDeprecated (IndexList& visibleObjects, const AABB* aabbs, size_t count, Umbra_3_0::Region* region)
{
    // TODO dynamic objects with legacy Tomes properly
	int visibleIndex = 0;
	for (int i=0;i<count;i++)
	{
		const AABB& bounds = aabbs[i];
		
		Vector3f mn = bounds.GetMin();
		Vector3f mx = bounds.GetMax();
		
        bool isVisible = region->isAABBVisible((Umbra_3_0::Vector3&)mn, (Umbra_3_0::Vector3&)mx);
		
		if (isVisible)
			visibleObjects[visibleIndex++] = i;
	}
	
	visibleObjects.size = visibleIndex;
}

static void CullDynamicObjectsWithoutUmbraInOut (IndexList& visibleObjects, const SceneCullingParameters& cullingParams, const AABB* aabbs)
{
	int visibleIndex = 0;
	for (int i=0;i<visibleObjects.size;i++)
	{
		int index = visibleObjects[i];
		const AABB& bounds = aabbs[index];
		if (IntersectAABBPlaneBounds(bounds, cullingParams.cullingPlanes, cullingParams.cullingPlaneCount))
			visibleObjects[visibleIndex++] = index;
	}
	
	visibleObjects.size = visibleIndex;
}

static void CullSceneWithLegacyUmbraDeprecated (SceneCullingParameters& cullingParams, CullingOutput& output, Umbra::Query* q, const Umbra_3_0::Tome* tome)
{
	///@TODO: Review if output.umbraVisibility makes any sense????
	
    Umbra_3_0::QueryExt*    query   = (Umbra_3_0::QueryExt*)q;
	
    query->init(tome);
	
	Umbra_3_0::Region* region;
	ALLOC_TEMP(region, Umbra_3_0::Region, 1);
	region->init();
	query->setVisibilityRegionOutput(region);
	
	
    Umbra_3_0::CameraTransform camera;
    if  (cullingParams.cullingPlaneCount > 6)
	    camera.setUserClipPlanes((Umbra_3_0::Vector4*)&cullingParams.cullingPlanes[6], cullingParams.cullingPlaneCount-6);
	
    camera.set((Umbra_3_0::Matrix4x4&)cullingParams.worldToClipMatrix, (Umbra_3_0::Vector3&)cullingParams.position);
	
    Umbra_3_0::IndexList staticObjs(output.visible[kStaticRenderers].indices, output.visible[kStaticRenderers].reservedSize);
	
    Umbra_3_0::Query::ErrorCode e;
	
    int flags = 0;
    if (tome->getStatistic(Umbra_3_0::Tome::STAT_PORTAL_DATA_SIZE))
        flags |= Umbra_3_0::Query::QUERYFLAG_PORTAL;
    else    // TODO: handle other cases?
        flags |= Umbra_3_0::Query::QUERYFLAG_PVS;
    
    e = query->queryCameraVisibility(flags, &staticObjs, NULL, camera);
	//Fallback to view frustum culling if queryCameraVisibility failed
	if (e != Umbra_3_0::Query::ERROR_OK)
	{
		cullingParams.useOcclusionCulling = false;
		cullingParams.useShadowCasterCulling = false;
		CullSceneWithoutUmbra(cullingParams, output);
		return;
	}
	output.visible[kStaticRenderers].size = staticObjs.getSize();
	
	// camera can move outside view volume Assert(e == Umbra::Query::ERROR_OK);
    // Extract visible indices
    if (!query->supportVFCulling())
		CullDynamicObjectsWithoutUmbraInOut (output.visible[kStaticRenderers], cullingParams, cullingParams.renderers[kStaticRenderers].bounds);

	//output.umbraVisibility->getOutputObjects()->setSize(output.visible[kStaticRenderers].size);

	ProcessIndexListIsNodeVisible(output.visible[kStaticRenderers], cullingParams.renderers[kStaticRenderers].nodes, cullingParams.renderers[kStaticRenderers].bounds, cullingParams);
	
	for (int i=kDynamicRenderer;i<kVisibleListCount;i++)
	{
		CullDynamicObjectsUmbraDeprecated (output.visible[i], cullingParams.renderers[i].bounds, cullingParams.renderers[i].rendererCount, region);
		ProcessIndexListIsNodeVisible(output.visible[i], cullingParams.renderers[i].nodes, cullingParams.renderers[i].bounds, cullingParams);
	}
}
#endif // SUPPORT_BACKWARDS_COMPATIBLE_UMBRA