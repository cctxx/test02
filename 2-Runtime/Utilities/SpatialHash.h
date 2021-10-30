#pragma once

#include "Runtime/Geometry/Plane.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/dense_hash_map.h"
#include <algorithm>

static const float kVoxelHeightMultiplier = 4.f;

struct PlaneData
{
			PlaneData () {}
			PlaneData (const Plane& plane, int colliderInstanceID, int rigidBodyOrColliderInstanceID)
			:	m_Plane (plane)
			,	m_ColliderInstanceID (colliderInstanceID)
			,	m_RigidBodyOrColliderInstanceID (rigidBodyOrColliderInstanceID)
			{}
	Plane	m_Plane;
	int		m_ColliderInstanceID;
	int		m_RigidBodyOrColliderInstanceID;
};

// Collision plane cache using the google dense hashmap
class PlaneColliderCache_dense_hashmap
{
public:
	PlaneColliderCache_dense_hashmap ();
	~PlaneColliderCache_dense_hashmap () {Clear();}
	int Size () const { return m_PlaneHashMap.size(); };
	bool Find (const Vector3f& pos, const Vector3f& dir, Plane& plane, int& colliderInstanceID, int& rigidBodyOrColliderInstanceID, float voxelSize) const;
	bool Replace (const Vector3f& pos, const Vector3f& dir, const Plane& plane, int colliderInstanceID, int rigidBodyOrColliderInstanceID, float voxelSize);
	void Clear ()
	{
		m_PlaneHashMap.clear ();
	}
private:
	// Hash map for cached planes
	struct UInt64HashFunctor
	{
		inline size_t operator ()(UInt64 x) const
		{
			return int(x >> 32);
		}
	};
	typedef std::pair<const UInt64, PlaneData> KeyToPlanePair;
	typedef dense_hash_map<UInt64, PlaneData, UInt64HashFunctor, std::equal_to<UInt64>, STL_ALLOCATOR (kMemSTL, KeyToPlanePair) > PlaneHashMap;
	PlaneHashMap m_PlaneHashMap;	
};

typedef PlaneColliderCache_dense_hashmap PlaneColliderCache;
