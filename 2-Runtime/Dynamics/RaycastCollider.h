#ifndef RAYCASTCOLLIDER_H
#define RAYCASTCOLLIDER_H

#include "Collider.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector2.h"


class RaycastCollider : public Collider
{
 public:	
	REGISTER_DERIVED_CLASS (RaycastCollider, Collider)
	DECLARE_OBJECT_SERIALIZE (RaycastCollider)
	
	RaycastCollider (MemLabelId label, ObjectCreationMode mode);
	
	virtual void Reset ();
	virtual void SmartReset ();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	
	float GetLength () const { return m_Length; }
	void SetLength (float f);
	
	Vector3f GetCenter () const { return m_Center; }
	void SetCenter (const Vector3f& center);

	float GetGlobalLength () const;
	Vector3f GetGlobalCenter () const;
	
	void TransformChanged (int changeMask);
	static void InitializeClass ();
	static void CleanupClass () {}
	Matrix4x4f CalculateTransform () const;

	protected:

	
	///@TODO ADD DIRECTION	
	virtual void FetchPoseFromTransform ();
	virtual bool GetRelativeToParentPositionAndRotation (Transform& transform, Transform& anyParent, Matrix4x4f& matrix);
	
	virtual void Create (const Rigidbody* ignoreAttachRigidbody);
	void ScaleChanged ();
	
	Vector3f m_Center;
	float    m_Length;///< range { 0, infinity }
};

#endif
