#ifndef SPHERECOLLIDER_H
#define SPHERECOLLIDER_H

#include "Collider.h"
#include "Runtime/Math/Vector3.h"


class SphereCollider : public Collider
{
 public:	
	REGISTER_DERIVED_CLASS (SphereCollider, Collider)
	DECLARE_OBJECT_SERIALIZE (SphereCollider)
	
	SphereCollider (MemLabelId label, ObjectCreationMode mode);
	
	virtual void Reset ();
	virtual void SmartReset ();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	
	void SetRadius (float r);
	float GetRadius () const { return m_Radius; }

	float GetScaledRadius () const;

	void SetCenter (const Vector3f& center);
	Vector3f GetCenter () { return m_Center; }

	Vector3f GetGlobalCenter () const;

	void TransformChanged (int changeMask);
	
	virtual AABB GetBounds ();
	
	protected:
	
	virtual void Create (const Rigidbody* ignoreAttachRigidbody);
	virtual void FetchPoseFromTransform ();
	virtual bool GetRelativeToParentPositionAndRotation (Transform& transform, Transform& anyParent, Matrix4x4f& matrix);

	virtual NxCCDSkeleton* CreateCCDSkeleton(float scale);
	
	void ScaleChanged ();
	
	/// The radius of the sphere. range { 0.00001, infinity }
	float m_Radius;
	Vector3f m_Center;

	#if UNITY_EDITOR
	/// In unity version 1.0 sphere radius did not change with scale.
	/// This was fixed with version 1.1
	/// In the transfer function we check if we should up to account for the now introduced scale.
	bool fixupSphereColliderBackwardsCompatibility;
	#endif
};

#endif
