#ifndef BOXCOLLIDER_H
#define BOXCOLLIDER_H

#include "Collider.h"
#include "Runtime/Math/Vector3.h"

class BoxCollider : public Collider
{
 public:	
	REGISTER_DERIVED_CLASS (BoxCollider, Collider)
	DECLARE_OBJECT_SERIALIZE (BoxCollider)
		
	BoxCollider (MemLabelId label, ObjectCreationMode mode);
			
	virtual void Reset ();
	virtual void SmartReset ();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	const Vector3f& GetSize () const { return m_Size; }
	void SetSize (const Vector3f& extents);

	const Vector3f& GetCenter () const { return m_Center; }
	void SetCenter (const Vector3f& pos);
	
	Vector3f GetGlobalExtents () const;
	Vector3f GetGlobalCenter () const;
	
	virtual void TransformChanged (int changeMask);

	protected:
	
	virtual void FetchPoseFromTransform ();
	virtual bool GetRelativeToParentPositionAndRotation (Transform& transform, Transform& anyParent, Matrix4x4f& matrix);

		
	virtual void Create (const Rigidbody* ignoreRigidbody);
	virtual void ScaleChanged ();

	virtual NxCCDSkeleton* CreateCCDSkeleton(float scale);
	
	Vector3f m_Center;
	Vector3f m_Size;
};

#endif
