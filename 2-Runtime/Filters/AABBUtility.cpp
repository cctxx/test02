#include "UnityPrefix.h"
#include "AABBUtility.h"
#include "Renderer.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Filters/Mesh/SpriteRenderer.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/BaseClasses/GameObject.h"

inline bool HasAABB (Renderer& renderer)
{
	return renderer.IsActive () && renderer.GetVisible ();
}

bool CalculateWorldAABB (GameObject& go, AABB* aabb)
{
	Renderer* renderer = go.QueryComponent (Renderer);
	if (renderer && HasAABB (*renderer))
	{
		renderer->GetWorldAABB ( *aabb );
		return true;
	}
	
	MeshFilter *lmf = go.QueryComponent (MeshFilter);
	if (lmf)
	{
		Mesh *lm = lmf->GetSharedMesh ();
		if (lm)
		{
			*aabb = lm->GetBounds ();
			Matrix4x4f matrix;
			go.GetComponent (Transform).CalculateTransformMatrix (matrix);
			TransformAABB (*aabb, matrix, *aabb);
			return true;
		}
	}

#if ENABLE_SPRITES
	SpriteRenderer* sprite = go.QueryComponent (SpriteRenderer);
	if (sprite)
	{
		Sprite* frame = sprite->GetSprite();
		if (frame)
		{
			*aabb = frame->GetBounds();
			Matrix4x4f matrix;
			go.GetComponent (Transform).CalculateTransformMatrix (matrix);
			TransformAABB (*aabb, matrix, *aabb);
			return true;
		}
	}
#endif

	aabb->SetCenterAndExtent( Vector3f::zero, Vector3f::zero );
	return false;	
}

bool CalculateLocalAABB (GameObject& go, AABB* aabb)
{
	Renderer* renderer = go.QueryComponent (Renderer);
	if (renderer && HasAABB (*renderer))
	{
		const TransformInfo& info = renderer->GetTransformInfo();

		Matrix4x4f transformWorldToLocal = renderer->GetComponent(Transform).GetWorldToLocalMatrix();
		Matrix4x4f rendererLocalToTransformLocal;
		MultiplyMatrices4x4(&transformWorldToLocal, &info.worldMatrix, &rendererLocalToTransformLocal);

		TransformAABB(info.localAABB, rendererLocalToTransformLocal, *aabb);
		
		return true;
	}
	
	MeshFilter *lmf = go.QueryComponent (MeshFilter);
	if (lmf)
	{
		Mesh *lm = lmf->GetSharedMesh ();
		if (lm)
		{
			*aabb = lm->GetBounds ();
			return true;
		}
	}
	
	aabb->SetCenterAndExtent( Vector3f::zero, Vector3f::zero );
	return false;	
}

bool CalculateAABBCornerVertices (GameObject& go, Vector3f* vertices)
{
	Renderer* renderer = go.QueryComponent (Renderer);
	if (renderer && HasAABB (*renderer))
	{
		Transform const& transform = renderer->GetTransform();

		AABB aabb;
		if (dynamic_pptr_cast<SkinnedMeshRenderer*> (renderer))
		{
			renderer->GetWorldAABB( aabb );
			aabb.GetVertices (vertices);
		}
		else
		{
			renderer->GetLocalAABB( aabb );

			aabb.GetVertices (vertices);
			TransformPoints3x4 (transform.GetLocalToWorldMatrix (), vertices, vertices, 8);
		}	
		
		return true;
	}
	
	Transform* transform = go.QueryComponent (Transform);
	MeshFilter *lmf = go.QueryComponent (MeshFilter);
	if (transform && lmf)
	{
		Mesh *lm = lmf->GetSharedMesh ();
		if (lm)
		{
			lm->GetBounds ().GetVertices (vertices);
			TransformPoints3x4 (transform->GetLocalToWorldMatrix (), vertices, vertices, 8);
			
			return true;
		}
	}
	
	return false;	
}

AABB CalculateWorldAABB (GameObject& go)
{
	AABB aabb;
	CalculateWorldAABB (go, &aabb);
	return aabb;
}
