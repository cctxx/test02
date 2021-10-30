#include "UnityPrefix.h"

#include "LODUtility.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Camera/LODGroup.h"
#include "Runtime/Filters/AABBUtility.h"
#include "Runtime/Camera/LODGroupManager.h"

void CalculateLODGroupBoundingBox ( LODGroup& group )
{
	Matrix4x4f worldToLocal = group.GetComponent(Transform).GetWorldToLocalMatrix();
	
	MinMaxAABB minmax;
	minmax.Init();
	for (int i=0;i<group.GetLODCount();i++)
	{
		for (int r=0;r<group.GetLOD(i).renderers.size();r++)
		{
			Renderer* renderer = group.GetLOD(i).renderers[r].renderer;
			if (renderer && renderer->GetGameObjectPtr())
			{
				AABB localBounds;
				if (CalculateLocalAABB (renderer->GetGameObject(), &localBounds))
				{
					Matrix4x4f relativeTransform;
					Matrix4x4f rendererLocalToWorld = renderer->GetTransform().GetLocalToWorldMatrix();
					
					MultiplyMatrices4x4(&worldToLocal, &rendererLocalToWorld, &relativeTransform);
					
					AABB lodGroupRelativeBoundds;
					TransformAABBSlow (localBounds, relativeTransform, lodGroupRelativeBoundds);

					minmax.Encapsulate(lodGroupRelativeBoundds);
				}
			}
		}
	}
	
	float size;
	if (minmax.IsValid())	
	{
		group.SetLocalReferencePoint (minmax.GetCenter());
		Vector3f extent = minmax.GetExtent() * 2.0F;
		size = std::max(std::max(extent.x, extent.y), extent.z);
	}
	else
	{
		group.SetLocalReferencePoint (Vector3f (0, 0, 0));
		size = 1;
	}
	
	float scale = group.GetWorldSpaceScale();
	if (scale > 0.0001F)
		size /= scale;

	group.SetSize (size);
}

void ForceLODLevel (const LODGroup& group, int index)
{
	int LODCount = group.GetLODCount();
	if (index >= LODCount)
	if (index >= LODCount)
	{
		WarningString("SetLODs: Attempting to force a LOD outside the number available LODs");
		return;
	}

	// mask of 0 = no force
	// now create a mask for the rest
	UInt32 lodMask = 0;
	if (index >= 0)
		lodMask = 1 << index;

	LODGroupManager& m = GetLODGroupManager();
	int lodGroupIndex = group.GetLODGroup();
	if (lodGroupIndex < 0)
	{
		WarningString("SetLODs: Attempting to force a LOD outside the number available LODs");
		return;
	}

	m.SetForceLODMask (lodGroupIndex, lodMask);
}

