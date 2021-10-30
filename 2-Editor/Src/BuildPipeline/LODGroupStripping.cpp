#include "UnityPrefix.h"
#include "Runtime/Camera/LODGroup.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Misc/GameObjectUtility.h"


void DestroyRenderersFromLODGroup (LODGroup& group, int maxLevel)
{
	LODGroup::LODArray lods;
	group.GetLODArray(lods);
	for (int i=0;i<std::min(maxLevel, group.GetLODCount());i++)
	{
		LODGroup::LOD& lod = lods[i];
		
		for (int r=0;r<lod.renderers.size();r++)
		{
			Renderer* renderer = lod.renderers[r].renderer;
			if (renderer == NULL)
				continue;

			UInt32 tempGroup;
			UInt32 lodMask;
			group.GetLODGroupIndexAndMask (renderer, &tempGroup, &lodMask);
			
			UInt8 supportedLODMask = 255;
			supportedLODMask >>= (8 - maxLevel);
			
			if (lodMask & supportedLODMask)
			{
				MeshFilter* filter = renderer->QueryComponent(MeshFilter);
				DestroyObjectHighLevel(renderer, false);
				DestroyObjectHighLevel(filter, false);
			}
		}
		lod.renderers.clear();
	}
	group.SetLODArray(lods);
}

void DestroyRenderersFromLODGroupInOpenScene (int maxLevel)
{
	if (maxLevel == 0)
		return;
	
	std::vector<LODGroup*> groups;
	Object::FindObjectsOfType(&groups);
	
	for (int i=0;i<groups.size();i++)
	{
		// Ignore prefabs
		////@TODO: Make a nice function for testing if an object is a scene object...
		if (groups[i]->IsPersistent() || groups[i]->TestHideFlag(Object::kDontSave))
			continue;
		
		DestroyRenderersFromLODGroup(*groups[i], maxLevel);
	}
}
