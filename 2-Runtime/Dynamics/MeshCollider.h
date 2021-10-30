#ifndef MESHCOLLIDER_H
#define MESHCOLLIDER_H

#include "Collider.h"
#include "Runtime/Math/Vector3.h"
class Mesh;

#if UNITY_EDITOR
class NxConvexMesh;
class NxTriangleMesh;
#endif

class MeshCollider : public Collider
{
public:	
	REGISTER_DERIVED_CLASS (MeshCollider, Collider)
	DECLARE_OBJECT_SERIALIZE (MeshCollider)

	MeshCollider (MemLabelId label, ObjectCreationMode mode);
	
	void SetSharedMesh (const PPtr<Mesh> m);
	PPtr<Mesh> GetSharedMesh ();
	
	void SetConvex (bool convex);
	bool GetConvex () const { return m_Convex; }

	void SetSmoothSphereCollisions (bool convex);
	bool GetSmoothSphereCollisions () const { return m_SmoothSphereCollisions; }
	
	virtual void Reset ();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	virtual void TransformChanged (int changeMask);
	static void InitializeClass ();
	static void CleanupClass () {}
	void DidDeleteMesh ();
	
	#if UNITY_EDITOR
	const NxConvexMesh* GetConvexMesh() const;
	const NxTriangleMesh* GetTriangleMesh() const;
	#endif

private:
	void CreateShape( void* nxmesh, const Rigidbody* ignoreRigidbody );
	
protected:
	
	virtual void Create (const Rigidbody* ignoreRigidbody);
	virtual void Cleanup ();
	virtual void ReCreate();
	void ScaleChanged ();

	//virtual NxCCDSkeleton* CreateCCDSkeleton(float scale);

	bool          m_SmoothSphereCollisions;
	bool          m_Convex;
	bool          m_Shared;
	PPtr<Mesh> m_Mesh;
	PPtr<Mesh> m_CachedMesh;
	ListNode<Object> m_MeshNode;
};

#endif
