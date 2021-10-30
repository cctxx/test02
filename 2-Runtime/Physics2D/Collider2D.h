#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "External/Box2D/Box2D/Box2D.h"

class Rigidbody2D;
class Transform;
class Matrix4x4f;
class PhysicsMaterial2D;


// --------------------------------------------------------------------------


class Collider2D : public Behaviour
{
public:
	typedef dynamic_array<b2Fixture*> FixtureArray;

public:
	REGISTER_DERIVED_ABSTRACT_CLASS (Collider2D, Behaviour)
	DECLARE_OBJECT_SERIALIZE (Collider2D)
	
	Collider2D (MemLabelId label, ObjectCreationMode mode);
	// virtual ~Collider2D (); declared-by-macro
	
	static void InitializeClass ();
	static void CleanupClass () {}

	virtual void Reset ();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void Deactivate (DeactivateOperation operation);
	void Cleanup ();

	// Colliders are recreated when deleting a rigibody component from existing game object;
	// since colliders now have to be attached to the "static body". Old RigidBody2D component
	// is not destroyed at this point yet, and thus it should be ignored when finding which
	// RB to attach colliders to.
	void RecreateCollider (const Rigidbody2D* ignoreRigidbody);

	virtual void AddToManager ();
	virtual void RemoveFromManager ();

	void SetIsTrigger (bool trigger);
	bool GetIsTrigger () const { return m_IsTrigger; }

	PPtr<PhysicsMaterial2D> GetMaterial ();
	void SetMaterial (PPtr<PhysicsMaterial2D> material);

	bool OverlapPoint (const Vector2f& point) const;

	// Shapes.
	inline int GetShapeCount() const { return m_Shapes.size(); }
	const b2Fixture* GetShape() const { return (m_Shapes.size() ? m_Shapes[0] : 0); }
	b2Fixture* GetShape() { return (m_Shapes.size() ? m_Shapes[0] : 0); }
	const FixtureArray& GetShapes() const { return m_Shapes; }
	FixtureArray& GetShapes() { return m_Shapes; }

	Rigidbody2D* GetRigidbody ();

	void TransformChanged (int changeMask);
	
protected:
	virtual void Create (const Rigidbody2D* ignoreRigidbody = NULL) = 0;
	void FinalizeCreate (b2FixtureDef& def, b2Body* body, const dynamic_array<b2Shape*>* shapes = 0);
	void CalculateColliderTransformation (const Rigidbody2D* ignoreRigidbody, b2Body** attachedBody, Matrix4x4f& matrix);

	bool HasLocalTransformChanged () const;
	void UpdateLocalTransform ();

protected:
	PPtr<PhysicsMaterial2D> m_Material;
	bool m_IsTrigger;
	bool m_IsStaticBody;
	FixtureArray m_Shapes;

private:
	Vector3f m_LocalPosition;
	Quaternionf m_LocalRotation;
	bool m_LocalTransformInitialized;
};

#endif
