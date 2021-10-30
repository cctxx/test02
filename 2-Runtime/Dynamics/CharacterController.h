#pragma once

#include "Runtime/BaseClasses/GameObject.h"
#include "Collider.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector2.h"

class NxCapsuleController;
struct MonoObject;
struct RootMotionData;

class CharacterController : public Collider
{
	public:
	
	REGISTER_DERIVED_CLASS(CharacterController, Collider)
	DECLARE_OBJECT_SERIALIZE(CharacterController)
		
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	CharacterController (MemLabelId label, ObjectCreationMode mode);
	virtual void Reset();
	virtual void SmartReset ();
	
	int Move (const Vector3f& movement);
	
	Vector2f GetGlobalExtents () const;
	
	virtual void TransformChanged(int mask);
	
	void SetRadius(float radius);
	float GetRadius() { return m_Radius; }
	void SetHeight(float height);
	float GetHeight() { return m_Height; }

	virtual AABB GetBounds ();

	Vector3f GetCenter ();
	void SetCenter(const Vector3f& center);

	float GetSlopeLimit ();
	void SetSlopeLimit (float limit);

	float GetStepOffset ();
	void SetStepOffset (float limit);

	virtual void SetIsTrigger (bool trigger);

	static void CreateControllerManager ();
	static void CleanupControllerManager ();
	
	Vector3f GetWorldCenterPosition() const;
	
	bool SimpleMove (const Vector3f& movement);
//	bool SimpleJump (const Vector3f& movement, float jumpHeight);
	
	Vector3f GetVelocity();
	
	bool IsGrounded ();
	int GetCollisionFlags() { return m_LastCollisionFlags; }

	bool GetDetectCollisions () { return m_DetectCollision; }
	void SetDetectCollisions (bool detect);

	static void InitializeClass ();
	static void CleanupClass ();

private:

	virtual void Create(const Rigidbody* ignoreAttachRigidbody);
	virtual void Cleanup();
	virtual void ScaleChanged ();
	
	void ApplyRootMotionBuiltin (RootMotionData* rootMotion);
	
	NxCapsuleController* m_Controller;
	
	float    m_MinMoveDistance;///< range { 0, infinity }
	float    m_SkinWidth;///< range { 0.0001, infinity }
	float    m_SlopeLimit;///< range { 0, 180 }
	float    m_StepOffset;///< range { 0, infinity }
	float    m_Height;///< range { 0, infinity }
	float    m_Radius;///< range { 0, infinity }
	Vector3f m_Center;
	bool m_DetectCollision;
	
	float    m_VerticalSpeed;
	Vector3f m_Velocity;
	Vector3f m_LastSimpleVelocity;
	
	int      m_LastCollisionFlags;
	
	friend class PhysicsManager;
};

struct ControllerColliderHit
{
	ScriptingObjectPtr        controller;
	ScriptingObjectPtr        collider;
	Vector3f           point;
	Vector3f           normal;
	Vector3f           motionDirection;
	float              motionLength;
	int              push;
};

struct ControllerControllerHit
{
	ScriptingObjectPtr          controller;
	ScriptingObjectPtr          other;
	short                push;
};
