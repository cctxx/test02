#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "RaycastHit.h"
#include "MeshCollider.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Terrain/Heightmap.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Dynamics/TerrainCollider.h"

Vector2f CalculateRaycastTexcoord (Collider* collider, const Vector2f& uv, const Vector3f& pos, UInt32 face, int texcoord)
{
	MeshCollider* meshCollider = dynamic_pptr_cast<MeshCollider*> (collider);
	if (meshCollider != NULL)
	{
		Mesh* mesh = meshCollider->GetSharedMesh();
		if (mesh == NULL)
			return Vector2f::zero;
		UInt32 indices[3];
		if (!mesh->ExtractTriangle (face, indices))
			return Vector2f::zero;
		StrideIterator<Vector2f> uvs;
		if (texcoord == 1 && mesh->IsAvailable (kShaderChannelTexCoord1))
			uvs = mesh->GetUvBegin (1);
		else if (mesh->IsAvailable (kShaderChannelTexCoord0))
			uvs = mesh->GetUvBegin (0);
		else
			return Vector2f::zero;
		Vector2f interpolated = uvs[indices[1]] * uv.x;
		interpolated += uvs[indices[2]] * uv.y;
		interpolated += uvs[indices[0]] * (1.0F - (uv.y + uv.x));
		return interpolated;
	}
#if ENABLE_TERRAIN
	TerrainCollider* terrainCollider = dynamic_pptr_cast<TerrainCollider*> (collider);
	if (terrainCollider)
	{
		Vector2f uv;
		Vector3f scale = terrainCollider->GetCachedInvSize();
		Vector3f transformPos = terrainCollider->GetComponent(Transform).GetPosition();
		uv.x = scale.x * (pos.x - transformPos.x);
		uv.y = scale.z * (pos.z - transformPos.z);
		return uv;
	}
	else
#endif // ENABLE_TERRAIN
		return Vector2f::zero;
}
#endif //ENABLE_PHYSICS
