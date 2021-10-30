#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Physics2D/Physics2DManager.h"
#include "Runtime/Physics2D/Physics2DSettings.h"

class Vector2f;
class Vector3f;
class Quaternionf;
class b2Body;
struct b2Vec2;
struct RootMotionData;


// --------------------------------------------------------------------------

enum RigidbodyInterpolation2D	{ kNoInterpolation2D = 0, kInterpolate2D = 1, kExtrapolate2D = 2 };
enum RigidbodySleepMode2D		{ kNeverSleep2D = 0, kStartAwake2D = 1, kStartAsleep2D = 2 };
enum CollisionDetectionMode2D	{ kDiscreteCollision2D = 0, kContinuousCollision2D = 1 };

// --------------------------------------------------------------------------


class Rigidbody2D : public Unity::Component
{
public:	
	REGISTER_DERIVED_CLASS (Rigidbody2D, Unity::Component)
	DECLARE_OBJECT_SERIALIZE (Rigidbody2D)
	
	Rigidbody2D (MemLabelId label, ObjectCreationMode mode);
	// virtual ~Rigidbody2D(); declared-by-macro

	static void InitializeClass ();
	static void CleanupClass () {}

	virtual void CheckConsistency ();
	virtual void Reset ();
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	virtual void Deactivate (DeactivateOperation operation);

	void Create ();

	void TransformChanged (int changeMask);
	void ApplyRootMotionBuiltin (RootMotionData* rootMotion);

	float GetDrag () const { return m_LinearDrag; }
	void SetDrag (float drag);

	float GetAngularDrag () const { return m_AngularDrag; }
	void SetAngularDrag (float drag);

	float GetGravityScale () const { return m_GravityScale; }
	void SetGravityScale (float scale);

	void SetIsKinematic (bool isKinematic);
	bool GetIsKinematic () const { return m_IsKinematic; }

	void SetFixedAngle (bool fixedAngle);
	bool IsFixedAngle () const { return m_FixedAngle; }

	RigidbodyInterpolation2D GetInterpolation () { return (RigidbodyInterpolation2D)m_Interpolate; }
	void SetInterpolation (RigidbodyInterpolation2D interpolation);

	RigidbodySleepMode2D GetSleepMode () { return (RigidbodySleepMode2D)m_SleepingMode; }
	void SetSleepMode (RigidbodySleepMode2D mode);

	CollisionDetectionMode2D GetCollisionDetectionMode () { return (CollisionDetectionMode2D)m_CollisionDetection; }
	void SetCollisionDetectionMode (CollisionDetectionMode2D mode);

	//  Get position and rotation.  This can be different from transform state when using interpolation.
	Vector3f GetBodyPosition () const;
	Quaternionf GetBodyRotation () const;

	// Linear velocity.
	Vector2f GetVelocity () const;
	void SetVelocity (const Vector2f& velocity);

	// Angular velocity.
	float GetAngularVelocity () const;
	void SetAngularVelocity (float velocity);

	// Sleeping.
	void SetSleeping (bool sleeping);
	bool IsSleeping () const;

	// Mass.
	float GetMass () const { return m_Mass; }
	void SetMass (float mass);
	void CalculateColliderBodyMass ();

	// Add forces.
	void AddForce (const Vector2f& force);
	void AddTorque (float torque);
	void AddForceAtPosition (const Vector2f& force, const Vector2f& position);
	
	inline b2Body* GetBody() { return m_Body; }

	static Rigidbody2D* FindRigidbody (const GameObject* gameObject, const Rigidbody2D* ignoreRigidbody = NULL);
	
private:
	void FetchPoseFromTransform (b2Vec2* outPos, float* outRot);
	void UpdateInterpolationInfo ();
	void Cleanup ();
	void InformCollidersOfNewBody ();
	void ReCreateInboundJoints ();
	void RetargetDependencies (Rigidbody2D* ignoreRigidBody);

private:
	b2Body* m_Body;
	Rigidbody2DInterpolationInfo* m_InterpolationInfo;

	float m_Mass;				///< The mass of the body.  range { 0.0001, 1000000 }
	float m_LinearDrag;			///< The linear drag coefficient. 0 means no damping. range { 0, 1000000 }
	float m_AngularDrag;		///< The angular drag coefficient. 0 means no damping. range { 0, 1000000 }
	float m_GravityScale;		///< How much gravity affects this body. range { -1000000, 1000000 }
	bool m_FixedAngle;			///< Whether the body's angle is fixed or not.
	bool m_IsKinematic;			///< Whether the body is kinematic or not.  If not then the body is dynamic.
	UInt8 m_Interpolate;		///< The per-frame update mode for the body.  enum { None = 0, Interpolate = 1, Extrapolate = 2 }
	UInt8 m_SleepingMode;		///< The sleeping mode for the body.  enum { Never Sleep = 0, Start Awake = 1, Start Asleep = 2 }
	UInt8 m_CollisionDetection;	///< The collision detection mode for the body.  enum { Discrete = 0, Continuous = 1 }
};

#endif
