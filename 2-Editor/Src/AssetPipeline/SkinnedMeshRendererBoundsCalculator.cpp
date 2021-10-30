#include "UnityPrefix.h"
#include "SkinnedMeshRendererBoundsCalculator.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"

void PrecalculateSkinnedMeshRendererBoundingVolumesRecurse (Transform& transform)
{
	SkinnedMeshRenderer* renderer = transform.QueryComponent(SkinnedMeshRenderer);
	if (renderer)
	{
		Assert(!renderer->GetUpdateWhenOffscreen());
		
		MinMaxAABB bounds;
		if (renderer->CalculateRootLocalSpaceBounds(bounds))
			renderer->SetLocalAABB(bounds);
		else
			renderer->SetUpdateWhenOffscreen(true);
	}
	
	for (Transform::iterator i=transform.begin();i != transform.end();++i)
	{
		PrecalculateSkinnedMeshRendererBoundingVolumesRecurse(**i);
	}
}

void PrecalculateSkinnedMeshRendererBoundingVolumes (GameObject& gameObject)
{
	PrecalculateSkinnedMeshRendererBoundingVolumesRecurse (gameObject.GetComponent(Transform));
}
