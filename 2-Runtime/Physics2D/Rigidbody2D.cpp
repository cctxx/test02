#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/Physics2D/RigidBody2D.h"
#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Physics2D/Joint2D.h"
#include "Runtime/Physics2D/Physics2DManager.h"

#include "Runtime/Graphics/Transform.h"
#include "Runtime/BaseClasses/MessageHandler.h"
#include "Runtime/GameCode/RootMotionData.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Math/Simd/math.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/ValidateArgs.h"

#include "External/Box2D/Box2D/Box2D.h"

IMPLEMENT_CLASS_HAS_INIT (Rigidbody2D)
IMPLEMENT_OBJECT_SERIALIZE (Rigidbody2D)


// --------------------------------------------------------------------------


Rigidbody2D::Rigidbody2D (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_Body(NULL)
,	m_InterpolationInfo(NULL)
{
}

Rigidbody2D::~Rigidbody2D ()
{
	Cleanup ();
}


void Rigidbody2D::InitializeClass ()
{
	REGISTER_MESSAGE (Rigidbody2D, kTransformChanged, TransformChanged, int);
	REGISTER_MESSAGE_PTR (Rigidbody2D, kAnimatorMoveBuiltin, ApplyRootMotionBuiltin, RootMotionData);
}


template<class TransferFunction>
void Rigidbody2D::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);

	TRANSFER (m_Mass);
	TRANSFER (m_LinearDrag);
	TRANSFER (m_AngularDrag);
	TRANSFER (m_GravityScale);
	TRANSFER (m_FixedAngle);
	TRANSFER (m_IsKinematic);
	TRANSFER (m_Interpolate);
	TRANSFER (m_SleepingMode);
	TRANSFER (m_CollisionDetection);
	transfer.Align();
}


void Rigidbody2D::CheckConsistency ()
{
	Super::CheckConsistency ();

	m_Mass = clamp<float> (m_Mass, 0.0001f, PHYSICS_2D_LARGE_RANGE_CLAMP);
	m_LinearDrag = clamp<float> (m_LinearDrag, 0.0f, PHYSICS_2D_LARGE_RANGE_CLAMP);
	m_AngularDrag = clamp<float> (m_AngularDrag, 0.0f, PHYSICS_2D_LARGE_RANGE_CLAMP);
	m_GravityScale = clamp<float> (m_GravityScale, -PHYSICS_2D_LARGE_RANGE_CLAMP, PHYSICS_2D_LARGE_RANGE_CLAMP);
	
	if (m_Interpolate>2)
		m_Interpolate = kNoInterpolation2D;

	if (m_SleepingMode>2)
		m_SleepingMode = kStartAwake2D;

	if (m_CollisionDetection!=0 && m_CollisionDetection!=1)
		m_CollisionDetection = kDiscreteCollision2D;
}


void Rigidbody2D::Reset ()
{
	Super::Reset ();

	m_Mass = 1.0f;
	m_LinearDrag = 0.0f;
	m_AngularDrag = 0.05f;
	m_GravityScale = 1.0f;
	m_FixedAngle = false;
	m_IsKinematic = false;
	m_Interpolate = (RigidbodyInterpolation2D)kNoInterpolation2D;
	m_SleepingMode = (RigidbodySleepMode2D)kStartAwake2D;
	m_CollisionDetection = (CollisionDetectionMode2D)kDiscreteCollision2D;
}


void Rigidbody2D::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	Assert (GameObject::GetMessageHandler().HasMessageCallback (ClassID(Rigidbody2D), kTransformChanged.messageID));

	// Create the body if it's not created already.
	if (IsActive() && m_Body == NULL)
		Create ();

	// Apply all body properties.
	// Note that we should not allow anything to adjust the current sleep-state of the body here when the component
	// is first woken otherwise we'll potentially override the need to be initially asleep (sleep mode).
	if (!(awakeMode & (kDidLoadFromDisk | kInstantiateOrCreateFromCodeAwakeFromLoad)))
	{
		SetMass (m_Mass);
		SetDrag (m_LinearDrag);
		SetAngularDrag (m_AngularDrag);
		SetGravityScale (m_GravityScale);
		SetIsKinematic (m_IsKinematic);
		SetFixedAngle (m_FixedAngle);
		SetCollisionDetectionMode ((CollisionDetectionMode2D)m_CollisionDetection);
		SetSleepMode ((RigidbodySleepMode2D)m_SleepingMode);
	}

	// Inform the colliders about the new body.
	if (awakeMode & kInstantiateOrCreateFromCodeAwakeFromLoad)
		InformCollidersOfNewBody ();
}


void Rigidbody2D::Deactivate (DeactivateOperation operation)
{
	Cleanup ();
}


void Rigidbody2D::Create ()
{
	if (m_Body != NULL)
		return;

	// Configure the body definitio0n.
	b2BodyDef bodyDef;
	bodyDef.type = m_IsKinematic ? b2_kinematicBody : b2_dynamicBody;
	bodyDef.userData = this;
	bodyDef.bullet = m_CollisionDetection == kContinuousCollision2D;
	bodyDef.linearDamping = m_LinearDrag;
	bodyDef.angularDamping = m_AngularDrag;
	bodyDef.gravityScale = m_GravityScale;
	bodyDef.fixedRotation = m_FixedAngle;
	bodyDef.allowSleep = m_SleepingMode != kNeverSleep2D;
	bodyDef.awake = m_SleepingMode != kStartAsleep2D;;

	// Fetch the body pose.
	if (IsActive ())
		FetchPoseFromTransform (&bodyDef.position, &bodyDef.angle);

	// Create the body.
	m_Body = GetPhysics2DWorld()->CreateBody (&bodyDef);

	// Calculate the collider body mass.
	CalculateColliderBodyMass ();

	// Set interpolation.
	SetInterpolation ((RigidbodyInterpolation2D)m_Interpolate);
}


void Rigidbody2D::TransformChanged (int changeMask)
{
	// Finish if no body exists.
	if (!m_Body)
		return;

	// Finish if transform message is disabled.
	if (!GetPhysics2DManager().IsTransformMessageEnabled())
		return;

	// Update the rigid-body transform if the position or rotation has changed.
	if (changeMask & (Transform::kPositionChanged | Transform::kRotationChanged))
	{
		b2Vec2 position;
		float angle;
		FetchPoseFromTransform (&position, &angle);
		m_Body->SetTransform (position, angle);
		m_Body->SetAwake(true);
		
		// Disable interpolation if transform has changed.
		if (m_InterpolationInfo)
			m_InterpolationInfo->disabled = 1;
	}

	// Recreate inbound joints if the scale has changed.
	if (changeMask & Transform::kScaleChanged)
	{
		ReCreateInboundJoints ();
		return;
	}
}


void Rigidbody2D::ApplyRootMotionBuiltin (RootMotionData* rootMotion)
{	
	if (m_Body == NULL || rootMotion->didApply)
		return;

	if(GetIsKinematic())
	{
		b2Vec2 position = m_Body->GetPosition ();
		position.x += rootMotion->deltaPosition.x;
		position.y += rootMotion->deltaPosition.y;
		m_Body->SetTransform (position, rootMotion->targetRotation.z);
	}
	else
	{
		Quaternionf rotation = GetComponent(Transform).GetRotation();
		Quaternionf invRotation = Inverse(rotation);

		// Get the physics velocity in local space
		const Vector2f velocity2 = GetVelocity();
		const Vector3f physicsVelocityLocal = RotateVectorByQuat(invRotation, Vector3f(velocity2.x, velocity2.y, 0.0f));

		// Get the local space velocity and blend it with the physics velocity on the y-axis based on gravity weight
		// We do this in local space in order to support moving on a curve earth with gravity changing direction
		Vector3f animVelocityGlobal = rootMotion->deltaPosition * GetInvDeltaTime();
		Vector3f localVelocity = RotateVectorByQuat (invRotation, animVelocityGlobal);
		localVelocity.y = Lerp (localVelocity.y, physicsVelocityLocal.y, rootMotion->gravityWeight);

		// If we use gravity, when we are in a jumping root motion, we have to cancel out the gravity
		// applied by default. When doing for example a jump the only thing affecting velocity should be the animation data.
		// The animation already has gravity applied in the animation data so to speak...
		if (GetGravityScale () > 0.0f)
			AddForce(GetPhysics2DSettings ().GetGravity() * -Lerp(1.0F, 0.0F, rootMotion->gravityWeight));

		// Apply velocity & rotation
		Vector3f globalVelocity = RotateVectorByQuat(rotation, localVelocity);
		m_Body->SetLinearVelocity (b2Vec2(globalVelocity.x, globalVelocity.y));
		const Vector3f localEuler = QuaternionToEuler (rootMotion->targetRotation);
		m_Body->SetTransform (m_Body->GetPosition (), localEuler.z);
	}	

	m_Body->SetAwake(true);
	rootMotion->didApply = true;
}


void Rigidbody2D::SetDrag (float drag)
{
	ABORT_INVALID_FLOAT (drag, drag, Rigidbody2D);

	m_LinearDrag = clamp<float> (drag, 0.0f, PHYSICS_2D_LARGE_RANGE_CLAMP);
	SetDirty ();

	if (m_Body)
		m_Body->SetLinearDamping (m_LinearDrag);
}


void Rigidbody2D::SetAngularDrag (float drag)
{
	ABORT_INVALID_FLOAT (drag, angularDrag, Rigidbody2D);

	m_AngularDrag = clamp<float> (drag, 0.0f, PHYSICS_2D_LARGE_RANGE_CLAMP);
	SetDirty ();

	if (m_Body)
		m_Body->SetAngularDamping (m_AngularDrag);
}


void Rigidbody2D::SetGravityScale (float scale)
{
	ABORT_INVALID_FLOAT (scale, gravityScale, Rigidbody2D);

	m_GravityScale = clamp<float> (scale, -PHYSICS_2D_LARGE_RANGE_CLAMP, PHYSICS_2D_LARGE_RANGE_CLAMP);
	SetDirty ();

	if (m_Body)
	{
		m_Body->SetGravityScale (m_GravityScale);
		
		// Wake body if a non-zero gravity is applied.
		if (m_GravityScale < 0.0f || m_GravityScale > 0.0f)
			m_Body->SetAwake (true);
	}
}


void Rigidbody2D::SetIsKinematic (bool isKinematic)
{
	m_IsKinematic = isKinematic;
	SetDirty();

	if (m_Body)
	{
		m_Body->SetType (m_IsKinematic ? b2_kinematicBody : b2_dynamicBody);

		// Wake the body.
		m_Body->SetAwake(true);

		// If transitioning into a kinematic body, reset velocities to match what 3D physics does.
		if (m_IsKinematic)
		{
			m_Body->SetLinearVelocity (b2Vec2_zero);
			m_Body->SetAngularVelocity (0.0f);
		}

		// Calculate the collider body mass.
		// NOTE: Unfortunately we must do this here as Box2D resets its mass-data when this is changed.
		CalculateColliderBodyMass ();
	}
}


void Rigidbody2D::SetFixedAngle (bool fixedAngle)
{
	m_FixedAngle = fixedAngle;
	SetDirty();

	if (m_Body)
	{
		m_Body->SetFixedRotation(m_FixedAngle);

		// Calculate the collider body mass.
		// NOTE: Unfortunately we must do this here as Box2D resets its mass-data when this is changed.
		CalculateColliderBodyMass ();
	}
}


void Rigidbody2D::SetInterpolation (RigidbodyInterpolation2D interpolation)
{
	m_Interpolate = interpolation;
	SetDirty();

	if (m_Body)
		UpdateInterpolationInfo();
}


void Rigidbody2D::SetSleepMode (RigidbodySleepMode2D mode)
{
	m_SleepingMode = mode;
	SetDirty ();

	if (m_Body)
		m_Body->SetSleepingAllowed (mode != kNeverSleep2D);
}


void Rigidbody2D::SetCollisionDetectionMode (CollisionDetectionMode2D mode)
{
	m_CollisionDetection = mode;
	SetDirty ();

	if (m_Body)
		m_Body->SetBullet( mode == kContinuousCollision2D );
}


Vector3f Rigidbody2D::GetBodyPosition () const
{
	Assert (m_Body != NULL);

	const b2Vec2& pos2D = m_Body->GetPosition();
	const Transform& transform = GetGameObject().GetComponent (Transform);
	Vector3f pos3D = transform.GetPosition();
	pos3D.x = pos2D.x;
	pos3D.y = pos2D.y;

	return pos3D;
}


Quaternionf Rigidbody2D::GetBodyRotation () const
{
	Assert (m_Body != NULL);

	return EulerToQuaternion (Vector3f(0, 0, m_Body->GetAngle()));
}


Vector2f Rigidbody2D::GetVelocity () const
{
	// Return no linear velocity if no body is available.
	if (m_Body == NULL)
		return Vector2f::zero;

	// Return the body linear velocity.
	const b2Vec2& velocity = m_Body->GetLinearVelocity ();
	return Vector2f (velocity.x, velocity.y);
}


void Rigidbody2D::SetVelocity (const Vector2f& velocity)
{
	ABORT_INVALID_VECTOR2 (velocity, velocity, Rigidbody2D);

	// Ignore linear velocity if no body is available.
	if (m_Body == NULL)
		return;

	m_Body->SetLinearVelocity( b2Vec2(velocity.x, velocity.y) );
}


float Rigidbody2D::GetAngularVelocity () const
{
	// Return no angular velocity if no body is available.
	if (m_Body == NULL)
		return 0.0f;

	// Return the body angular velocity.
	return math::degrees (m_Body->GetAngularVelocity());
}


void Rigidbody2D::SetAngularVelocity (float velocity)
{
	ABORT_INVALID_FLOAT (velocity, angularVelocity, Rigidbody2D);

	// Ignore linear velocity if no body is available.
	if (m_Body == NULL)
		return;

	m_Body->SetAngularVelocity (math::radians (velocity));
}


void Rigidbody2D::SetSleeping (bool sleeping)
{
	// Ignore sleeping if no body is available.
	if (m_Body == NULL)
		return;

	m_Body->SetAwake(!sleeping);
}


bool Rigidbody2D::IsSleeping() const
{
	// Not sleeping if no body is available.
	if (m_Body == NULL)
		return false;

	return !m_Body->IsAwake();
}


void Rigidbody2D::SetMass (float mass)
{
	ABORT_INVALID_FLOAT (mass, mass, Rigidbody2D);

	// Clamp mass.
	m_Mass = clamp<float> (mass, 0.0001f, PHYSICS_2D_LARGE_RANGE_CLAMP);

	// Calculate the collider body mass.
	CalculateColliderBodyMass ();
}


void Rigidbody2D::CalculateColliderBodyMass ()
{
	// Finish if body is not dynamic or has no fixtures.
	if (m_Body == NULL ||
		m_Body->GetType () != b2_dynamicBody)
		return;

	// Reset the mass-data.
	m_Body->ResetMassData ();

	// Scale body mass to target mass.
	b2MassData bodyMassData;
	m_Body->GetMassData (&bodyMassData);
	const float massScale = m_Mass / bodyMassData.mass;
	bodyMassData.mass *= massScale;

	// Scale or set rotational inertia.
	if (m_Body->GetFixtureCount () > 0)
		bodyMassData.I *= massScale;
	else
		bodyMassData.I = m_Mass * 10.0f;

	m_Body->SetMassData (&bodyMassData);
}


void Rigidbody2D::AddForce (const Vector2f& force)
{
	ABORT_INVALID_ARG_VECTOR2 (force, force, AddForce, Rigidbody2D);

	// Ignore force if no body is available.
	if (m_Body == NULL)
		return;

	m_Body->ApplyForceToCenter (b2Vec2(force.x,force.y), true);
}


void Rigidbody2D::AddTorque (float torque)
{
	ABORT_INVALID_ARG_FLOAT (torque, torque, AddTorque, Rigidbody2D);

	// Ignore torque if no body is available.
	if (m_Body == NULL)
		return;

	m_Body->ApplyTorque (torque, true);
}


void Rigidbody2D::AddForceAtPosition (const Vector2f& force, const Vector2f& position)
{
	ABORT_INVALID_ARG_VECTOR2 (force, force, AddForceAtPosition, Rigidbody2D);
	ABORT_INVALID_ARG_VECTOR2 (position, position, AddForceAtPosition, Rigidbody2D);

	// Ignore force-at-position if no body is available.
	if (m_Body == NULL)
		return;

	m_Body->ApplyForce (b2Vec2(force.x, force.y), b2Vec2(position.x, position.y), true);
}


Rigidbody2D* Rigidbody2D::FindRigidbody (const GameObject* gameObject, const Rigidbody2D* ignoreRigidbody)
{
	Assert (gameObject != NULL);

	// If there's a rigid-body on this game-object then return it.
	Rigidbody2D* rigidBody = gameObject->QueryComponent (Rigidbody2D);
	if (rigidBody && rigidBody != ignoreRigidbody && rigidBody->IsActive ())
		return rigidBody;

	// Search for a rigid-body up the transform hierarchy.
	Transform* parent = gameObject->GetComponent (Transform).GetParent ();
	while (parent)
	{
		GameObject* parentGameObject = parent->GetGameObjectPtr ();
		if (parentGameObject)
			rigidBody = parentGameObject->QueryComponent (Rigidbody2D);
		else
			rigidBody = NULL;

		if (rigidBody && rigidBody != ignoreRigidbody && rigidBody->IsActive ())
			return rigidBody;

		parent = parent->GetParent ();
	}

	// Nothing found!
	return NULL;
}


// --------------------------------------------------------------------------


void Rigidbody2D::FetchPoseFromTransform (b2Vec2* outPos, float* outRot)
{
	Transform& transform = GetComponent (Transform);
	Vector3f pos = transform.GetPosition ();
	Quaternionf rot = transform.GetRotation ();
	
	AssertFiniteParameter(pos)
	AssertFiniteParameter(rot)
	
	float rotZ = QuaternionToEuler(rot).z;
	outPos->Set (pos.x, pos.y);
	*outRot = rotZ;
}


void Rigidbody2D::UpdateInterpolationInfo ()
{
	// Remove the interpolation info if no interpolation selected.
	if (m_Interpolate == kNoInterpolation2D)
	{
		UNITY_DELETE(m_InterpolationInfo, kMemPhysics);
		return;
	}

	// Finish if we already have interpolation info.
	if (m_InterpolationInfo != NULL)
		return;

	// Generate the interpolation info.
	m_InterpolationInfo = UNITY_NEW(Rigidbody2DInterpolationInfo, kMemPhysics);
	Rigidbody2DInterpolationInfo& info = *m_InterpolationInfo;
	info.body = this;
	info.disabled = 1;
	info.position = Vector3f::zero;
	info.rotation = Quaternionf::identity();
	GetPhysics2DManager().GetInterpolatedBodies().push_back(*m_InterpolationInfo);
}


void Rigidbody2D::Cleanup ()
{
	if (m_Body == NULL)
		return;

	// Process all colliders as the rigid-body is going away.
	const int fixtureCount = m_Body->GetFixtureCount ();
	if (fixtureCount > 0 )
	{
		dynamic_array<Collider2D*> attachedColliders(kMemTempAlloc);

		// Gather all attached colliders.
		for (b2Fixture* fixture = m_Body->GetFixtureList (); fixture != NULL; fixture = fixture->GetNext ())
		{
			Collider2D* collider = (Collider2D*)fixture->GetUserData ();
			attachedColliders.push_back (collider);
		}

		// Process all colliders.
		for (dynamic_array<Collider2D*>::iterator colliderItr = attachedColliders.begin (); colliderItr != attachedColliders.end (); ++colliderItr)
			(*colliderItr)->Cleanup ();
	}

	// Process all joints as the rigid-body is going away.
	if (m_Body->GetJointList () != NULL)
	{
		dynamic_array<Joint2D*> attachedJoints(kMemTempAlloc);

		// Gather all joints.
		for (b2JointEdge* joints = m_Body->GetJointList(); joints != NULL; joints = joints->next)
			attachedJoints.push_back ((Joint2D*)joints->joint->GetUserData());

		// Process all joints.
		for (dynamic_array<Joint2D*>::iterator jointItr = attachedJoints.begin (); jointItr != attachedJoints.end (); ++jointItr)
			(*jointItr)->Cleanup ();
	}

	// Destroy the body.
	GetPhysics2DWorld()->DestroyBody (m_Body);
	m_Body = NULL;

	// Destroy the body interpolation information.
	UNITY_DELETE(m_InterpolationInfo, kMemPhysics);
}


void Rigidbody2D::InformCollidersOfNewBody ()
{
	// Fetch all potential colliders.
	dynamic_array<Unity::Component*> colliders (kMemTempAlloc);
	GetComponentsInChildren (GetGameObject (), false, ClassID (Collider2D), colliders);

	// Finish if no colliders are found.
	if (colliders.size () == 0)
		return;

	// Recreate the colliders.
	for (dynamic_array<Unity::Component*>::iterator colliderItr = colliders.begin (); colliderItr != colliders.end (); ++colliderItr)
	{
		Collider2D* collider = (Collider2D*)*colliderItr;

		// Ignore if not enabled.
		if (!collider->GetEnabled ())
			continue;
		
		collider->RecreateCollider (NULL);
	}
}


void Rigidbody2D::ReCreateInboundJoints ()
{
	// Finish if not appropriate.
	if (m_Body == NULL)
		return;

	// Fetch the joints this body is connected to.
	// This can occur when there's "inbound" joints from other components on other game objects.
	b2JointEdge* joints = m_Body->GetJointList();

	// Process all joints.
	while (joints != NULL)
	{
		// Fetch the joint.
		b2Joint* joint = joints->joint;

		// Fetch the next joint.
		b2JointEdge* nextJoint = joints->next;

		// Fetch the joint component.
		Joint2D* jointComponent = (Joint2D*)joint->GetUserData();
		Assert (jointComponent != NULL);

		// Recreate the joint.
		jointComponent->ReCreate ();

		// Next joint.
		joints = nextJoint;
	}
}

#endif // #if ENABLE_2D_PHYSICS
