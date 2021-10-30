#include "UnityPrefix.h"
#include "SpatialHash.h"
#include "External/MurmurHash/MurmurHash2.h"
#include "Runtime/Geometry/AABB.h"
#include <float.h>


// quantisation of direction
inline unsigned QuantiseDirection(const Vector3f& dir)
{
	// quantise direction	
	unsigned id;
	if (fabsf(dir.x) >= fabsf(dir.y) && fabsf(dir.x) >= fabsf(dir.z))
		id = dir.x > 0 ? 0 : 1;
	else if (fabsf(dir.y) >= fabsf(dir.z))
		id = dir.y > 0 ? 2 : 3;
	else
		id = dir.z > 0 ? 4 : 5;
	return id;
}

// quantisation of position
inline void QuantisePosition(const Vector3f& pos, float voxelSize, int* xyz)
{
	// quantise position
	const float cell_side = voxelSize;
	const float cell_height = voxelSize * kVoxelHeightMultiplier;
	xyz[0] = (int)std::floor(pos.x / cell_side);
	xyz[1] = (int)std::floor(pos.y / cell_height);
	xyz[2] = (int)std::floor(pos.z / cell_side);
}

inline AABB QuantisedValueToAABB(int* quantisedValue, float voxelSize)
{
	const float cell_side = voxelSize;
	const float cell_height = voxelSize * kVoxelHeightMultiplier;
	const Vector3f pos(quantisedValue[0]*cell_side,quantisedValue[1]*cell_height,quantisedValue[2]*cell_side);	
	const Vector3f halfDir( cell_side*.5f, cell_height*.5f, cell_side*.5f );
	return AABB(pos + halfDir, Abs(halfDir));
}

// compute hash index for directions cache
inline UInt64 ComputeHash(const Vector3f& pos, const Vector3f& dir, float voxelSize )
{
	int qp[3];
	QuantisePosition(pos, voxelSize, qp);
	UInt64 key = MurmurHash64A( (void*)qp, sizeof(int)*3, 0xdeadbeef186726feULL );
	// quantise direction	
	unsigned id = QuantiseDirection(dir);
	key = key ^ id;
	return key;
}

/////////////////////////////////////////////////////
// Collision plane cache members
PlaneColliderCache_dense_hashmap::PlaneColliderCache_dense_hashmap()
{
	m_PlaneHashMap.set_empty_key (ComputeHash(Vector3f(FLT_MAX,FLT_MAX,FLT_MAX),Vector3f(FLT_MAX,FLT_MAX,FLT_MAX),1.f));
	m_PlaneHashMap.set_deleted_key (ComputeHash(Vector3f(FLT_MIN,FLT_MIN,FLT_MIN),Vector3f(FLT_MIN,FLT_MIN,FLT_MIN),1.f));
}

bool PlaneColliderCache_dense_hashmap::Replace (const Vector3f& pos, const Vector3f& dir, const Plane& plane, int colliderInstanceID, int rigidBodyOrColliderInstanceID, float voxelSize)
{
	UInt64 hash = ComputeHash (pos, dir, voxelSize);
	int sz = Size();	
	m_PlaneHashMap.insert (std::make_pair (hash, PlaneData (plane, colliderInstanceID, rigidBodyOrColliderInstanceID)));
	if ( sz >= Size() )
	{
		PlaneHashMap::iterator it = m_PlaneHashMap.find (hash);
		if (it == m_PlaneHashMap.end ())
			return false;	
		it->second.m_Plane = plane;
		it->second.m_ColliderInstanceID = colliderInstanceID;
		it->second.m_RigidBodyOrColliderInstanceID = rigidBodyOrColliderInstanceID;
	}
	return true;
}

bool PlaneColliderCache_dense_hashmap::Find (const Vector3f& pos, const Vector3f& dir, Plane& plane, int& colliderInstanceID, int& rigidBodyOrColliderInstanceID, float voxelSize) const
{
	UInt64 hash = ComputeHash (pos, dir, voxelSize);
	PlaneHashMap::const_iterator it = m_PlaneHashMap.find (hash);
	if (it == m_PlaneHashMap.end ())
		return false;	
	plane = it->second.m_Plane;
	colliderInstanceID = it->second.m_ColliderInstanceID;
	rigidBodyOrColliderInstanceID = it->second.m_RigidBodyOrColliderInstanceID;
	return true;
}

