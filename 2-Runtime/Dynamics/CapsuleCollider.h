#ifndef CAPSULECOLLIDER_H
#define CAPSULECOLLIDER_H

#include "Collider.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector2.h"

class CapsuleCollider : public Collider
{
 public:	
	REGISTER_DERIVED_CLASS (CapsuleCollider, Collider)
	DECLARE_OBJECT_SERIALIZE (CapsuleCollider)
	
	CapsuleCollider (MemLabelId label, ObjectCreationMode mode);
	
	virtual void Reset ();
	virtual void SmartReset ();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	
	void SetRadius (float radius);
	float GetRadius () const { return m_Radius; }
	
	float GetHeight () const { return m_Height; }
	void SetHeight (float height);

	Vector3f GetCenter () const { return m_Center; }
	void SetCenter (const Vector3f& center);
	
	int GetDirection () const { return m_Direction; }
	void SetDirection (int dir);

	Vector2f GetGlobalExtents () const;
	Vector3f GetGlobalCenter () const;

	virtual void TransformChanged (int changeMask);
	
	Matrix4x4f CalculateTransform () const;

	virtual AABB GetBounds ();
	
	protected:
	
	
	virtual void FetchPoseFromTransform ();
	virtual bool GetRelativeToParentPositionAndRotation (Transform& transform, Transform& anyParent, Matrix4x4f& matrix);
	
	virtual void Create (const Rigidbody* ignoreRigidbody);
	void ScaleChanged ();

	virtual NxCCDSkeleton* CreateCCDSkeleton(float scale);
	
	float m_Radius;///< range { 0, infinity }
	float m_Height;///< range { 0, infinity }
	int   m_Direction;///< enum { X-Axis = 0, Y-Axis = 1, Z-Axis = 2 }
	
	Vector3f m_Center;
};

#endif
