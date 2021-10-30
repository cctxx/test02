#ifndef RIGIDBODY_H
#define RIGIDBODY_H

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Math/Matrix4x4.h"
#include "PhysicsManager.h"

class Transform;
class Quaternionf;
struct RootMotionData;

namespace Unity { class Joint; }

enum RigidbodyInterpolation { kNoInterpolation = 0, kInterpolate = 1, kExtrapolate = 2 };


class Rigidbody : public Unity::Component {
 public:	
	REGISTER_DERIVED_CLASS (Rigidbody, Component)
	DECLARE_OBJECT_SERIALIZE (Rigidbody)
	
	Rigidbody (MemLabelId label, ObjectCreationMode mode);
	// virtual ~Rigidbody(); declared-by-macro
	virtual void Reset ();

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	
	virtual void Deactivate (DeactivateOperation operation);
	
	static void InitializeClass ();
	static void CleanupClass () {}

	// Is the rigid body affected by gravity?	
	void SetUseGravity (bool gravity);
	bool GetUseGravity () const;

	// Mass of the rigid body
	void SetMass (float mass);
	float GetMass () const;

	// Center of Mass of the rigid body in local space
	void SetCenterOfMass (const Vector3f& centerOfMass);
	Vector3f GetCenterOfMass () const;

	// Center of Mass of the rigid body in world space
	Vector3f GetWorldCenterOfMass () const;
	
	// The rotation of the inertia tensor	
	void SetInertiaTensorRotation (const Quaternionf& inertia);
	Quaternionf GetInertiaTensorRotation () const;

	// The diagonal inertia tensor of mass relative to the center of mass and inertia rotation
	void SetInertiaTensor (const Vector3f& inertia);
	Vector3f GetInertiaTensor () const;

	// Does the rigid body modify the rotation?
	void SetFreezeRotation (bool freezeRotation);
	bool GetFreezeRotation () const;

	void SetConstraints (int flags);
	int GetConstraints () const;

	void SetIsKinematic (bool isKinematic);
	bool GetIsKinematic () const;
	
	void SetCollisionDetectionMode (int ccd);
	int GetCollisionDetectionMode () const { return m_CollisionDetection; }

	void SetDensity (float density);
	
	//  Get Position and rotation - This can be different from transform state when using interpolation
	Vector3f GetPosition () const;
	Quaternionf GetRotation () const;

	// Set pos&rot this will cause transform.position/rotation to be updated delayed after the next fixed step
	void SetPosition (const Vector3f& p);
	void SetRotation (const Quaternionf& q);

	/// Move to a position
	/// This happens one frame delayed.
	void MovePosition (const Vector3f& pos);
	void MoveRotation (const Quaternionf& rot);

	// Velocity	
	Vector3f GetVelocity () const;
	void SetVelocity (const Vector3f& velocity);

	// Angular velocity
	Vector3f GetAngularVelocity () const;
	void SetAngularVelocity (const Vector3f& velocity);

	/// The linear drag coefficient. 0 means no drag. range { 0, infinity }
	float GetDrag () const;
	void SetDrag (float damping);

	// The angular drag coefficient. 0 means no drag. range { 0, infinity }
	void SetAngularDrag (float damping);
	float GetAngularDrag () const;
	
	/// Lets you set the maximum angular velocity permitted for this rigid body. Because for various computations, the rotation
	/// of an object is linearized, quickly rotating actors introduce error into the simulation, which leads to bad things.
	///
	/// However, because some rigid bodies, such as car wheels, should be able to rotate quickly, you can override the default setting
	/// on a per-rigid body basis with the below call. Note that objects such as wheels which are approximated with spherical or 
	/// other smooth collision primitives can be simulated with stability at a much higher angular velocity than, say, a box that
	/// has corners.
	void SetMaxAngularVelocity (float maxAngularVelocity);
	float GetMaxAngularVelocity () const;
	
	// The velocity of a point given in world coordinates
	// if it were attached to the actor and moving with it.
	Vector3f GetPointVelocity (const Vector3f& worldPoint) const;

	// The velocity of a point given in world coordinates
	// if it were attached to the actor and moving with it.
	Vector3f GetRelativePointVelocity (const Vector3f& localPoint) const;
	
	enum ForceMode
	{
		FORCE,                   ///< parameter has unit of mass * distance/ time^2, i.e. a force
		IMPULSE,                 ///< parameter has unit of mass * distance /time
		VELOCITY_CHANGE,         ///< parameter has unit of distance / time, i.e. the effect is mass independent: a velocity change.
		SMOOTH_IMPULSE,          ///< same as NX_IMPULSE but the effect is applied over all substeps.  Use this for motion controllers that repeatedly apply an impulse.
		SMOOTH_VELOCITY_CHANGE,  ///< same as NX_VELOCITY_CHANGE but the effect is applied over all substeps.  Use this for motion controllers that repeatedly apply an impulse.
		ACCELERATION             ///< parameter has unit of distance/ time^2, i.e. an acceleration.  It gets treated just like a force except the mass is not divided out before integration.
	};

	enum { kCCDModeOff = 0, kCCDModeNormal = 1, kCCDModeDynamic = 2 };
	
	enum {
		kFreezeNone = 0,
		kFreezePositionX = (1<<1),
		kFreezePositionY = (1<<2),
		kFreezePositionZ = (1<<3),
		kFreezeRotationX = (1<<4),
		kFreezeRotationY = (1<<5),
		kFreezeRotationZ = (1<<6),
		kFreezePosition = kFreezePositionX | kFreezePositionY | kFreezePositionZ,
		kFreezeRotation = kFreezeRotationX | kFreezeRotationY | kFreezeRotationZ,
		kFreezeAll = kFreezePosition | kFreezeRotation,
	};

	// Applies a force (or impulse) defined in the global coordinate frame,
	// acting at a particular point in global coordinates on the rigid body. 
	// Note that if the force does not act along the center of mass of the actor, this
	// will also add the corresponding torque.
	// Forces should be applied inside FixedUpdate only, because forces are reset at the end of every fixed timestep.
	void AddForceAtPosition (const Vector3f& force, const Vector3f& position, int mode = FORCE);

	// Applies a force (or impulse) defined in the global coordinate frame,
	// This will not induce torque.
	// Forces should be applied inside FixedUpdate only, because forces are reset at the end of every fixed timestep.
	void AddForce (const Vector3f& force, int mode = FORCE);

	// Applies a force (or impulse) defined in the local coordinate frame,
	// This will not induce torque.
	// Forces should be applied inside FixedUpdate only, because forces are reset at the end of every fixed timestep.
	void AddRelativeForce (const Vector3f& force, int mode = FORCE);

	// Applies an (eventually impulsive) torque defined in the global coordinate frame to the actor.
	// Torques should be applied inside FixedUpdate only, because forces are reset at the end of every fixed timestep.
	void AddTorque (const Vector3f& torque, int mode = FORCE);

	// Applies an (eventually impulsive) torque defined in the local coordinate frame to the actor.
	// Torques should be applied inside FixedUpdate only, because forces are reset at the end of every fixed timestep.
	void AddRelativeTorque (const Vector3f& torque, int mode = FORCE);
	
	
	void AddExplosionForce (float force, const Vector3f& position, float radius, float upwardsModifier, int forceMode = FORCE);
	void ClosestPointOnBounds (const Vector3f& position, Vector3f& outPosition, float& outSqrDistance);
	
	bool GetDetectCollisions() const;
	void SetDetectCollisions(bool enable);
	
	RigidbodyInterpolation GetInterpolation () { return (RigidbodyInterpolation)m_Interpolate; }
	void SetInterpolation (RigidbodyInterpolation interpolation);

	bool GetUseConeFriction () const;
	void SetUseConeFriction (bool cone);

	bool IsSleeping ();
	void Sleep ();
	void WakeUp ();
	
	void SetSolverIterationCount (int iterationCount);
	int GetSolverIterationCount () const;
	
	void SetSleepVelocity (float value);
	float GetSleepVelocity () const;

	void SetSleepAngularVelocity (float value);
	float GetSleepAngularVelocity () const;

	void TransformChanged (int mask);
	void ApplyRootMotionBuiltin (RootMotionData* rootMotion);
	
	bool SweepTest (const Vector3f &direction, float distance, RaycastHit& outHit);
	const PhysicsManager::RaycastHits& SweepTestAll (const Vector3f &direction, float distance);

	virtual void CheckConsistency ();
	
	// Testing API
	NxActor* GetActor() { return m_Actor; }
	
	private:

	void SortParentedRigidbodies ();
	void FetchPoseFromTransform ();
	virtual void SupportedMessagesDidChange (int supported);
	void UpdateSortedBody ();

	void Create (bool active);
	void CleanupInternal (bool recreateColliders);
	void UpdateInterpolationNode ();

	void UpdateMassDistribution ();
	ListNode<Rigidbody> m_SortedNode;
	class NxActor* m_Actor;

	float m_Mass;///< The mass of the body. range { 0.0000001, 1000000000 }
	float m_Drag;///< The linear drag coefficient. 0 means no damping. range { 0, infinity }
	float m_AngularDrag;  ///< The angular drag coefficient. 0 means no damping. range { 0, infinity }

	UInt8 m_ActiveScene;
	UInt8 m_ImplicitTensor;
	
	bool m_UseGravity;
	bool m_IsKinematic;
	int m_Constraints;
	int m_CollisionDetection;///< enum { Discrete = 0, Continuous = 1, Continuous Dynamic = 2 }
	int m_CachedCollisionDetection; 
	
	public:

	/// This is used to prevent read back from kinematic rigidbodies.
	/// * setGlobalPosition sets the position immediately. No triggers will be activated.
	///   No velocity applied -> thus no friction for rigidbodies sitting on animated objects.
	/// * moveGlobalPosition sets the position delayed during simulate. Simply calling moveGlobalPosition for all kinematic objects
	///   doesn't work because then the physics update loop will update delayed, causing small epsilon precision errors to accumulate
	///   from the local to world, then world to local transformation. If the transform which has the kinematic rigidbody attached
	///   is not actually animated, the ragdoll will slowly drift apart.
	///   On top of that, it is slow since we read back the position twice for animated ragdolls and characters.
	/// ->> The solution is to store if we should read back the update.
	///     If we are setting the position directly from a Transform update, there is no reason to update the transform
	///     Also the world position wont change during simulation since it is kinematic rigidbody.
	

	UInt8 m_Interpolate;///< enum { None = 0, Interpolate = 1, Extrapolate = 2 }
	int m_DisableReadUpdateTransform;
	int m_DisableInterpolation;
	RigidbodyInterpolationInfo* m_InterpolationInfo;
	
	friend class Collider;
	friend class Unity::Joint;
	friend class PhysicsManager;
};

/// Notifications:
/// bool RigidbodyChanged (const Rigidbody& b);
extern MessageIdentifier kRigidbodyChanged;



#endif
