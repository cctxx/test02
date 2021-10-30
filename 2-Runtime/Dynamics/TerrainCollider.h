#pragma once

#if ENABLE_TERRAIN && ENABLE_PHYSICS

#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Dynamics/Collider.h"

class TerrainData;

class TerrainCollider : public Collider
{
	PPtr<TerrainData>			m_TerrainData;
	bool                      m_CreateTreeColliders;
	ListNode<TerrainCollider> m_Node;
	std::vector<NxShape*>		m_TreeColliders;
	Vector3f					m_CachedInvSize;
	
	public:

	TerrainCollider (MemLabelId label, ObjectCreationMode mode);
	
	REGISTER_DERIVED_CLASS(TerrainCollider, Collider)
	DECLARE_OBJECT_SERIALIZE(TerrainCollider)
	
	void ScaleChanged (){}
	void FetchPoseFromTransform ();
	
	Vector3f GetCachedInvSize () { return m_CachedInvSize; }
	
	void SetTerrainData (PPtr<TerrainData> map);
	TerrainData* GetTerrainData ();
	
	void CreateTrees ();
	virtual void TransformChanged (int changeMask);	
	void TerrainChanged (int changeMask);	
	virtual void Create (const Rigidbody* ignoreAttachRigidbody);
	virtual void Cleanup ();
	static void InitializeClass ();
	static void CleanupClass () {}

	bool HasShape() const { return m_Shape != NULL; }
	virtual bool SupportsMaterial () const { return false; }
};

#endif