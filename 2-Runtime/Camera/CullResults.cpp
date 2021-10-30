#include "UnityPrefix.h"
#include "CullResults.h"
#include "ShadowCulling.h"//@TODO: Remove
#include "UmbraBackwardsCompatibility.h"

CullResults::CullResults() : shadowCullData(NULL), treeSceneNodes(kMemTempAlloc), treeBoundingBoxes(kMemTempAlloc)
{
}

CullResults::~CullResults()
{
	Umbra::Visibility* umbra = sceneCullingOutput.umbraVisibility;
	if (umbra != NULL)
	{
		int* clusterBuffer = (int*)umbra->getOutputClusters()->getPtr();
		UNITY_FREE(kMemTempAlloc, clusterBuffer);

		Umbra::IndexList* outputObjects = umbra->getOutputObjects();
		Umbra::OcclusionBuffer* outputBuffer = umbra->getOutputBuffer();
		Umbra::IndexList* outputCluster = umbra->getOutputClusters();
		
		UNITY_DELETE(outputObjects, kMemTempAlloc);
		UNITY_DELETE(outputBuffer, kMemTempAlloc);
		UNITY_DELETE(outputCluster, kMemTempAlloc);
		UNITY_DELETE(umbra, kMemTempAlloc);
	}
	
	DestroyCullingOutput(sceneCullingOutput);

	UNITY_DELETE(shadowCullData, kMemTempAlloc);
}

void InitIndexList (IndexList& list, size_t count)
{
	int* array = (int*)UNITY_MALLOC (kMemTempAlloc, count * sizeof(int));
	list = IndexList (array, 0, count);
}

void DestroyIndexList (IndexList& list)
{
	UNITY_FREE(kMemTempAlloc, list.indices);
	list.indices = NULL;
}

void CreateCullingOutput (const RendererCullData* rendererCullData, CullingOutput& cullingOutput)
{
	for(int i=0;i<kVisibleListCount;i++)
		InitIndexList(cullingOutput.visible[i], rendererCullData[i].rendererCount);
}

void DestroyCullingOutput (CullingOutput& cullingOutput)
{
	for (int i=0;i<kVisibleListCount;i++)
		DestroyIndexList (cullingOutput.visible[i]);
}

void CullResults::Init (const UmbraTomeData& tomeData, const RendererCullData* rendererCullData)
{
	CreateCullingOutput(rendererCullData, sceneCullingOutput);
	
	if (tomeData.tome != NULL)
	{
		size_t staticCount = rendererCullData[kStaticRenderers].rendererCount;
	   	Assert(staticCount == 0 || staticCount == UMBRA_TOME_METHOD(tomeData, getObjectCount()));
		size_t clusterCount = UMBRA_TOME_METHOD(tomeData, getClusterCount());
		
		int* clusterArray = (int*)UNITY_MALLOC(kMemTempAlloc, clusterCount * sizeof(int));
		
		Umbra::IndexList* staticList = UNITY_NEW(Umbra::IndexList (sceneCullingOutput.visible[kStaticRenderers].indices, staticCount), kMemTempAlloc);
		Umbra::OcclusionBuffer* occlusionBuffer = UNITY_NEW(Umbra::OcclusionBuffer, kMemTempAlloc);
		Umbra::Visibility* umbraVisibility = UNITY_NEW(Umbra::Visibility(staticList, occlusionBuffer), kMemTempAlloc);
		Umbra::IndexList* clusterList = UNITY_NEW(Umbra::IndexList (clusterArray, clusterCount), kMemTempAlloc);
		umbraVisibility->setOutputClusters(clusterList);
		
		sceneCullingOutput.umbraVisibility = umbraVisibility;
	}
	else
	{
		sceneCullingOutput.umbraVisibility = NULL;
	}
}
