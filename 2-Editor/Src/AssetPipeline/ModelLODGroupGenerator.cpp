#include "UnityPrefix.h"
#include "Runtime/Camera/LODGroupManager.h"
#include "Runtime/Camera/LODGroup.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Utilities/LODUtility.h"

const char* kLODPrefix = "_LOD";

static int ExtractLODName (const char* name)
{
	name = strstr(name, kLODPrefix);
	if (name)
	{
		name += strlen(kLODPrefix);
		if (IsDigit(*name))
		{
			return atoi(name);
		}
	}
	return -1;
}


static void ApplyLODGroupFromObjectNames (Transform& transform, LODGroup::LODArray& lods)
{
	Renderer* renderer = transform.QueryComponent(Renderer);
	if (renderer != NULL)
	{
		int lod = ExtractLODName(transform.GetName());
		if (lod >=0 && lod < lods.size())
			lods[lod].renderers.push_back().renderer = renderer;
	}
	
	for (int i=0;i<transform.GetChildrenCount();i++)
		ApplyLODGroupFromObjectNames(transform.GetChild(i), lods);
}

std::string GenerateLODGroupFromModelImportNamingConvention (GameObject& gameObject, std::vector<float>& screenPercentages)
{
	LODGroup::LODArray lods;
	lods.resize(kMaximumLODLevels);
	
	if (gameObject.QueryComponent(Transform))
		ApplyLODGroupFromObjectNames (gameObject.GetComponent(Transform), lods);
	
	int activeLODSize = 0;
	for (int i=1;i<lods.size();i++)
	{
		if (!lods[i].renderers.empty())
			activeLODSize = i + 1;
		
		if (!lods[i].renderers.empty() && lods[i-1].renderers.empty())
			return Format("Inconsistent LOD naming (%s%d found but no %s%d).", kLODPrefix, i, kLODPrefix, i-1);
	}
	
	if (activeLODSize == 0)
		return "";

	lods.resize(activeLODSize);

	if (activeLODSize == screenPercentages.size ())
	{
		for (int i=0;i<lods.size();i++)
		{
			lods[i].screenRelativeHeight = screenPercentages[i];
		}
	}
	else
	{
		// Setup some default values for LOD group screen relative height.
		screenPercentages.resize(activeLODSize);
		float size = 0.5F;
		for (int i=0;i<lods.size()-1;i++)
		{
			size /= 2;
			lods[i].screenRelativeHeight = size;
			screenPercentages[i] = size;
		}
		// Contribution culling when the object is smaller than 5 pixels
		lods.back().screenRelativeHeight = 0.01F;
		screenPercentages.back() = 0.01F;
	}
	
	LODGroup* lodGroup = dynamic_pptr_cast<LODGroup*> (AddComponent(gameObject, "LODGroup"));
	if (lodGroup == NULL)
		return "Failed to add LODGroup component";
	
	lodGroup->SetLODArray(lods);
	CalculateLODGroupBoundingBox (*lodGroup);
	
	return "";
}


#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
SUITE ( ModuleLODGroupGeneratorTests )
{
TEST (ExtractLODName)
{
	CHECK(ExtractLODName("mystuff_LOD3") == 3);
	CHECK(ExtractLODName("_LOD0") == 0);
	CHECK(ExtractLODName("mystuff_LOD") == -1);
	CHECK(ExtractLODName("mystuff_LOD\0_LOD2") == -1);
	CHECK(ExtractLODName("LOD0") == -1);
}
}

#endif
