#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "RigidBody.h"
#include "Collider.h"
#include "Joint.h"
#include "PhysicsManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "NxWrapperUtility.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/BaseClasses/MessageHandler.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Utilities/ValidateArgs.h"
#include "Runtime/GameCode/RootMotionData.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Misc/BuildSettings.h"

using namespace Unity;
using namespace std;

PROFILER_INFORMATION(gSweepTestProfile, "Rigidbody.SweepTest", kProfilerPhysics)
PROFILER_INFORMATION(gSweepTestAllProfile, "Rigidbody.SweepTestAll", kProfilerPhysics)

/*
 - Rigid bodies can't change the inertia tensor
 - Transfer function should be optimize to serialize into desc data if rigid body doesn't exit yet (Possibly always)
 
 - Make it so you can have multiple rigidbodies inside of each other and they update in the right order!
   What about sleeping??????
 
*/


inline Quaternionf QuatFromNx(const NxQuat& q)
{
        return Quaternionf(q.x, q.y, q.z, q.w);
}



Rigidbody::Rigidbody (MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode),
	m_SortedNode(this)
{
	m_Actor = NULL;
	m_ImplicitTensor = true;
	m_InterpolationInfo = NULL;
	m_DisableReadUpdateTransform = 0;
	m_CollisionDetection = kCCDModeOff;
	m_CachedCollisionDetection = kCCDModeOff;
}

Rigidbody::~Rigidbody ()
{
	CleanupInternal (false);
}

void Rigidbody::Reset ()
{
	Super::Reset ();

	if (m_Actor)
	{
		SetMass(1.0F);
		SetAngularDrag(0.05f);
		SetDrag(0.0F);
		SetConstraints(kFreezeNone);
		SetIsKinematic(false);
		SetUseGravity(true);
		SetCollisionDetectionMode(kCCDModeOff);
	}
	else
	{
		m_Mass = 1.0F;
		m_AngularDrag = 0.05f;
		m_Drag = 0.0f;
		m_Constraints = kFreezeNone;
		m_IsKinematic = false;
		m_UseGravity = true;
		m_CollisionDetection = kCCDModeOff;
		m_CachedCollisionDetection = kCCDModeOff;
	}
	m_Interpolate = kNoInterpolation;
}

void Rigidbody::CheckConsistency ()
{
	Super::CheckConsistency ();
	m_Mass = max(0.0000001F, m_Mass);
}

// Because Component -> Deactivate can be called but IsActive might still return true, we need to pass the active state!
void Rigidbody::Create (bool isActive)
{
	if (m_Actor == NULL || (bool)m_ActiveScene != isActive)
	{
		NxBodyDesc bodyDesc;
		bodyDesc.solverIterationCount = GetPhysicsManager ().GetSolverIterationCount ();
		NxActorDesc actorDesc;
		
		if (m_Actor)
		{
			m_Actor->saveBodyToDesc (bodyDesc);
			CleanupInternal (true);
		}
		else
		{
			bodyDesc.massSpaceInertia = NxVec3 (1.0F, 1.0F, 1.0F);
			bodyDesc.mass = m_Mass;
			bodyDesc.linearDamping = m_Drag;
			bodyDesc.angularDamping = m_AngularDrag;
			bodyDesc.mass = m_Mass;
			if (!m_UseGravity)
				bodyDesc.flags |= NX_BF_DISABLE_GRAVITY;
			if (m_IsKinematic)
				bodyDesc.flags |= NX_BF_KINEMATIC;
			bodyDesc.flags |= m_Constraints;
		}
		actorDesc.body = &bodyDesc;
		actorDesc.userData = this;

		if (isActive)
		{
			//;;printf_console ("Creating active physics actor\n");
			m_Actor = GetDynamicsScene ().createActor (actorDesc);
			SupportedMessagesDidChange (GetGameObject ().GetSupportedMessages ());
			m_ActiveScene = true;
		}
		else
		{
			//;;printf_console ("Creating inactive physics actor\n");
			m_Actor = GetInactiveDynamicsScene ().createActor (actorDesc);
		
			m_ActiveScene = false;
		}

		UpdateInterpolationNode();
	}
	
	if (!isActive && m_Actor != NULL)
	{
		Assert(m_Actor->getNbShapes() == 0);
	}
}


void Rigidbody::SupportedMessagesDidChange (int supported)
{
	if (m_Actor)
	{
		if (supported & kHasCollisionStay)
			m_Actor->setGroup (kContactTouchGroup);
		else if (supported & (kHasCollisionStay | kHasCollisionEnterExit))
			m_Actor->setGroup (kContactEnterExitGroup);
		else
			m_Actor->setGroup (kContactNothingGroup);
	}
}

void Rigidbody::CleanupInternal (bool recreateColliders)
{
	if (m_Actor)
	{
		int shapeCount = m_Actor->getNbShapes ();
		NxShape*const * shapes = m_Actor->getShapes ();
		
		Collider** colliders;
		ALLOC_TEMP(colliders, Collider*, shapeCount)

		for (int i=0;i<shapeCount;i++)
		{
			Collider* collider = (Collider*)shapes[i]->userData;
			AssertIf (collider == NULL);
			colliders[i] = collider;
			collider->Cleanup ();
		}
		
		if (m_ActiveScene)
		{
			//;;printf_console ("Deleting active physics actor");
			GetDynamicsScene ().releaseActor (*m_Actor);
		}
		else
		{
			//;;printf_console ("Deleting inactive physics actor");
			GetInactiveDynamicsScene ().releaseActor (*m_Actor);
		}
		m_Actor = NULL;
		
		if (recreateColliders)
		{
			for (int i=0;i<shapeCount;i++)
			{
				colliders[i]->RecreateCollider (this);
			}
		}

		delete m_InterpolationInfo;
		m_InterpolationInfo = NULL;
		m_CachedCollisionDetection = m_CollisionDetection;
	}
	Assert(m_Actor == NULL);
	m_SortedNode.RemoveFromList();
}

Vector3f Rigidbody::GetWorldCenterOfMass () const
{
	return Vec3FromNx (m_Actor->getCMassGlobalPosition());
}

void Rigidbody::Deactivate (DeactivateOperation operation)
{
	// When we are about to destroy a rigidbody, we don't destroy it immediately
	// Instead we destroy it in the rigidbody destructor.
	// This way scripts have no chance of accessing the colliders.
	if (operation == kWillDestroyGameObjectDeactivate)
		;
	else
	{	
		Create (false);
	}
	m_SortedNode.RemoveFromList();
	
	Super::Deactivate (operation);
}

void Rigidbody::UpdateSortedBody ()
{
	m_SortedNode.RemoveFromList();
	if (m_ActiveScene)
		GetPhysicsManager().AddBody(GetTransformDepth(GetComponent(Transform)), m_SortedNode); 
}

void Rigidbody::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	
	AssertIf (GameObject::GetMessageHandler ().HasMessageCallback (ClassID (Rigidbody), kTransformChanged.messageID) == false);
	
	Create (IsActive ());
	
	// When modifying already loaded rigidbody
	// Apply properties through immediate mode function
	if (!(awakeMode & kDidLoadFromDisk))
	{
		SetIsKinematic(m_IsKinematic);
		SetMass(m_Mass);
		SetDrag(m_Drag);
		SetAngularDrag(m_AngularDrag);
		SetUseGravity(m_UseGravity);
		SetCollisionDetectionMode(m_CollisionDetection);
		SetConstraints(m_Constraints);
	}
	
	UpdateInterpolationNode();
	
	if (IsActive())
		FetchPoseFromTransform();
	
	if (!GetIsKinematic())
		m_DisableReadUpdateTransform = 0;
	
	if (awakeMode & kActivateAwakeFromLoad)
	{
		MessageData data;
		GetComponent(Transform).BroadcastMessageAny(kForceRecreateCollider, data);
	}
	
	UpdateSortedBody ();
}

void Rigidbody::ClosestPointOnBounds (const Vector3f& position, Vector3f& outPosition, float& outSqrDistance)
{
	// No collider - just the distance to the center of mass
	int count = m_Actor->getNbShapes();
	if (count == 0)
	{
		outPosition = GetWorldCenterOfMass();
		outSqrDistance = SqrMagnitude(position - outPosition);
		return;
	}
	
	outSqrDistance = std::numeric_limits<float>::infinity();

	NxShape*const * shapes = m_Actor->getShapes();
	for (int i=0;i<count;i++)
	{
		NxBounds3 bounds;
		shapes[i]->getWorldBounds(bounds);
		AABB aabb;
		bounds.getCenter((NxVec3&)aabb.GetCenter());
		bounds.getExtents((NxVec3&)aabb.GetExtent());
		
		Vector3f closest;
		float sqrDistance;
		
		CalculateClosestPoint(position, aabb, closest, sqrDistance);
		if (sqrDistance < outSqrDistance)
		{
			outPosition = closest;
			outSqrDistance = sqrDistance;
		}
	}
}

void Rigidbody::AddExplosionForce (float force, const Vector3f& position, float radius, float upwardsModifier, int forceMode)
{
	Vector3f pointOnSurface;
	float sqrDistance;

	Vector3f offsetPosition = position - Vector3f(0.0F, upwardsModifier, 0.0F);
	if (upwardsModifier == 0.0F)
	{
		ClosestPointOnBounds(position, pointOnSurface, sqrDistance);
	}
	else
	{
		/// Upwards modifier will not modify the distance
		/// But it will modify the point on the surface
		ClosestPointOnBounds(position, pointOnSurface, sqrDistance);

		float tmpDistance;
		ClosestPointOnBounds(offsetPosition, pointOnSurface, tmpDistance);
	}
	
	// Linear distance fall off
	float distanceScale;
	if (radius > Vector3f::epsilon)
		distanceScale = 1.0F - clamp01(sqrtf(sqrDistance) / radius);
	else
		distanceScale = 1.0F;
	
	// Calculate normalized direction towards surface point
	Vector3f direction = pointOnSurface - offsetPosition;
	
	float length = Magnitude(direction);
	if (length > Vector3f::epsilon)
		direction /= length;
	else
		direction = Vector3f (0.0F, 1.0F, 0.0F);
	
	AddForceAtPosition(force * distanceScale * direction, pointOnSurface, forceMode);
}

void Rigidbody::InitializeClass ()
{
	REGISTER_MESSAGE (Rigidbody, kTransformChanged, TransformChanged, int);
	REGISTER_MESSAGE_PTR (Rigidbody, kAnimatorMoveBuiltin, ApplyRootMotionBuiltin, RootMotionData);
}

template<class TransferFunction>
void Rigidbody::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	transfer.SetVersion (2);
	TRANSFER_SIMPLE (m_Mass);
	TRANSFER_SIMPLE (m_Drag);
	TRANSFER_SIMPLE (m_AngularDrag);
	TRANSFER_SIMPLE (m_UseGravity);
	TRANSFER (m_IsKinematic);

	transfer.Transfer (m_Interpolate, "m_Interpolate");
	if (transfer.IsOldVersion(1))
	{
		bool freezeRotation;
		transfer.Transfer (freezeRotation, "m_FreezeRotation");
		if (freezeRotation)
			m_Constraints = kFreezeRotation;
		else
			m_Constraints = kFreezeNone;
	}
	else
	{
		transfer.Align();

		// Hide in editor and show using custom inspector instead.
		transfer.Transfer (m_Constraints, "m_Constraints", kHideInEditorMask | kGenerateBitwiseDifferences);
	}
	
	TRANSFER (m_CollisionDetection);
}

void Rigidbody::SetUseGravity (bool value)
{
	AssertIf (m_Actor == NULL);
	SetDirty ();
	if (value)
	{
		m_Actor->clearBodyFlag (NX_BF_DISABLE_GRAVITY);
		m_Actor->wakeUp();
	}
	else
		m_Actor->raiseBodyFlag (NX_BF_DISABLE_GRAVITY);
	m_UseGravity = value;
}

bool Rigidbody::GetUseGravity () const
{
	AssertIf (m_Actor == NULL);
	return !m_Actor->readBodyFlag (NX_BF_DISABLE_GRAVITY);
}

void Rigidbody::SetFreezeRotation (bool value)
{
	if (value)
		SetConstraints (GetConstraints() | kFreezeRotation);
	else
		SetConstraints (GetConstraints() & ~kFreezeRotation);
}

bool Rigidbody::GetFreezeRotation () const
{
	return (GetConstraints () & kFreezeRotation) == kFreezeRotation;
}

void Rigidbody::SetConstraints (int value)
{
	AssertIf (m_Actor == NULL);

	// If we are removing constraints, wake up the rigidbody, so it can start moving.
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_3_a1) && m_Constraints & ~value)
		WakeUp();
	
	SetDirty ();
	value &= NX_BF_FROZEN;
	m_Actor->clearBodyFlag (NX_BF_FROZEN);
	m_Actor->raiseBodyFlag ((NxBodyFlag)value);
	m_Constraints = value;
}

int Rigidbody::GetConstraints () const
{
	AssertIf (m_Actor == NULL);
	
	int value = 0;
	for (int i = kFreezePositionX; i <= kFreezeRotationZ; i<<=1)
	{
		if (m_Actor->readBodyFlag ((NxBodyFlag)i))
			value |= i;
	}
	return value;
}

void Rigidbody::SetIsKinematic (bool value)
{
	AssertIf (m_Actor == NULL);

	SetDirty ();
	if (value)
		m_Actor->raiseBodyFlag (NX_BF_KINEMATIC);
	else
		m_Actor->clearBodyFlag (NX_BF_KINEMATIC);
	m_IsKinematic = value;
	UpdateInterpolationNode ();
	
	m_DisableReadUpdateTransform = 0;
}

bool Rigidbody::GetIsKinematic () const
{
	AssertIf (m_Actor == NULL);
	return m_Actor->readBodyFlag (NX_BF_KINEMATIC);
}

void Rigidbody::UpdateMassDistribution ()
{
	AssertIf (m_Actor == NULL);

	if (m_ImplicitTensor)
	{	
		NxShape*const * shapes = m_Actor->getShapes();
		int count = m_Actor->getNbShapes();
		for (int i=0;i<count;i++)
		{
			Collider* coll = (Collider*)(shapes[i]->userData);
			
			// Triggers don't have mass, and RaycastColliders are too thing to give good mathematical useful results.
			if (!shapes[i]->getFlag(NX_TRIGGER_ENABLE) && !(coll && coll->GetClassID() == 140))
			{
				m_Actor->updateMassFromShapes (0.0F, m_Mass);		
				return;
			}
		}

		// no usable shapes - reset actor CoG and inertia
		m_Actor->setMass (m_Mass);
		m_Actor->setCMassOffsetLocalPosition ((const NxVec3&)Vector3f::zero);
		m_Actor->setMassSpaceInertiaTensor ((const NxVec3&)Vector3f::one);		
	}	
}

void Rigidbody::SetMass (float mass)
{
	AssertIf (m_Actor == NULL);
	SetDirty ();
	m_Mass = mass;

	if (m_ImplicitTensor)
		UpdateMassDistribution();
	else
		m_Actor->setMass (mass);
}

float Rigidbody::GetMass () const
{
	AssertIf (m_Actor == NULL);
	return m_Actor->getMass ();
}

void Rigidbody::SetCenterOfMass (const Vector3f& centerOfMass)
{
	m_Actor->setCMassOffsetLocalPosition ((const NxVec3&)centerOfMass);
	m_ImplicitTensor = false;
}

Vector3f Rigidbody::GetCenterOfMass () const
{
	return Vec3FromNx(m_Actor->getCMassLocalPosition ());
}

void Rigidbody::SetInertiaTensorRotation (const Quaternionf& inertia)
{
	m_ImplicitTensor = false;
	NxMat33 matrix ((const NxQuat&)inertia);
	m_Actor->setCMassOffsetLocalOrientation (matrix);
}

Quaternionf Rigidbody::GetInertiaTensorRotation () const
{
	NxMat33 matrix = m_Actor->getCMassLocalOrientation ();
	NxQuat quat (matrix);
	return (const Quaternionf&)quat;
}

void Rigidbody::SetInertiaTensor (const Vector3f& inertia)
{
	m_ImplicitTensor = false;
	if (inertia.x > std::numeric_limits<float>::epsilon() &&
		inertia.y > std::numeric_limits<float>::epsilon() &&
		inertia.z > std::numeric_limits<float>::epsilon())
		m_Actor->setMassSpaceInertiaTensor ((const NxVec3&)inertia);
	else
		ErrorStringObject ("Inertia tensor must be larger then zero in all coordinates.", this);
}

Vector3f Rigidbody::GetInertiaTensor () const
{
	return Vec3FromNx(m_Actor->getMassSpaceInertiaTensor ());
}

Vector3f Rigidbody::GetVelocity () const
{
	AssertIf (m_Actor == NULL);
	return Vec3FromNx(m_Actor->getLinearVelocity ());
}

void Rigidbody::SetVelocity (const Vector3f& velocity)
{
	AssertIf (m_Actor == NULL);
	ABORT_INVALID_VECTOR3 (velocity, velocity, rigidbody);
	m_Actor->setLinearVelocity ((const NxVec3&)velocity);
}

Vector3f Rigidbody::GetAngularVelocity () const
{
	AssertIf (m_Actor == NULL);
	return Vec3FromNx(m_Actor->getAngularVelocity ());
}

void Rigidbody::SetAngularVelocity (const Vector3f& velocity)
{
	AssertIf (m_Actor == NULL);
	ABORT_INVALID_VECTOR3 (velocity, angularVelocity, rigidbody);
	m_Actor->setAngularVelocity ((const NxVec3&)velocity);
}

void Rigidbody::SetDrag (float damping)
{
	AssertIf (m_Actor == NULL);
	SetDirty ();
	m_Drag = damping;
	m_Actor->setLinearDamping (damping);
}

float Rigidbody::GetDrag () const
{
	AssertIf (m_Actor == NULL);
	return m_Actor->getLinearDamping ();
}

void Rigidbody::SetAngularDrag (float damping)
{
	AssertIf (m_Actor == NULL);
	SetDirty ();
	m_AngularDrag = damping;
	m_Actor->setAngularDamping (damping);
}

float Rigidbody::GetAngularDrag () const
{
	AssertIf (m_Actor == NULL);
	return m_Actor->getAngularDamping ();
}

void Rigidbody::SetMaxAngularVelocity (float maxAngularVelocity)
{
	AssertIf (m_Actor == NULL);
	return m_Actor->setMaxAngularVelocity (maxAngularVelocity);
}

float Rigidbody::GetMaxAngularVelocity () const
{
	AssertIf (m_Actor == NULL);
	return m_Actor->getMaxAngularVelocity ();
}

void Rigidbody::SetSolverIterationCount (int iterationCount)
{
	return m_Actor->setSolverIterationCount(iterationCount);	
}

int Rigidbody::GetSolverIterationCount () const
{
	return m_Actor->getSolverIterationCount();	
}

void Rigidbody::SetSleepVelocity (float value)
{
	m_Actor->setSleepLinearVelocity(value);	
}

float Rigidbody::GetSleepVelocity () const
{
	return m_Actor->getSleepLinearVelocity();	
}

void Rigidbody::SetSleepAngularVelocity (float value)
{
	m_Actor->setSleepAngularVelocity(value);	
}

float Rigidbody::GetSleepAngularVelocity () const
{
	return m_Actor->getSleepAngularVelocity();	
}

void Rigidbody::SetInterpolation (RigidbodyInterpolation interpolation)
{
	m_Interpolate = interpolation;
	UpdateInterpolationNode();
}

Vector3f Rigidbody::GetPointVelocity (const Vector3f& worldPoint) const
{
	AssertIf (m_Actor == NULL);
	return Vec3FromNx(m_Actor->getPointVelocity ((const NxVec3&)worldPoint));
}

Vector3f Rigidbody::GetRelativePointVelocity (const Vector3f& localPoint) const
{
	AssertIf (m_Actor == NULL);
	return Vec3FromNx(m_Actor->getLocalPointVelocity ((const NxVec3&)localPoint));
}

void Rigidbody::AddForceAtPosition (const Vector3f& force, const Vector3f& position, int mode)
{
	ABORT_INVALID_VECTOR3 (force, force, rigidbody)
	ABORT_INVALID_VECTOR3 (position, position, rigidbody)
	AssertIf (m_Actor == NULL);
	if (!m_Actor->readBodyFlag(NX_BF_KINEMATIC))
		m_Actor->addForceAtPos ((const NxVec3&)force, (const NxVec3&)position, (NxForceMode)mode);
}

void Rigidbody::AddForce (const Vector3f& force, int mode)
{
	ABORT_INVALID_VECTOR3 (force, force, rigidbody)
	AssertIf (m_Actor == NULL);
	if (!m_Actor->readBodyFlag(NX_BF_KINEMATIC))
		m_Actor->addForce ((const NxVec3&)force, (NxForceMode)mode);
}

void Rigidbody::AddRelativeForce (const Vector3f& force, int mode)
{
	ABORT_INVALID_VECTOR3 (force, force, rigidbody)
	AssertIf (m_Actor == NULL);
	if (!m_Actor->readBodyFlag(NX_BF_KINEMATIC))
		m_Actor->addLocalForce ((const NxVec3&)force, (NxForceMode)mode);
}

void Rigidbody::AddTorque (const Vector3f& torque, int mode)
{
	ABORT_INVALID_VECTOR3 (torque, torque, rigidbody)
	AssertIf (m_Actor == NULL);
	if (!m_Actor->readBodyFlag(NX_BF_KINEMATIC))
		m_Actor->addTorque ((const NxVec3&)torque, (NxForceMode)mode);
}

void Rigidbody::AddRelativeTorque (const Vector3f& torque, int mode)
{
	ABORT_INVALID_VECTOR3 (torque, torque, rigidbody)
	AssertIf (m_Actor == NULL);
	if (!m_Actor->readBodyFlag(NX_BF_KINEMATIC))
		m_Actor->addLocalTorque ((const NxVec3&)torque, (NxForceMode)mode);
}

void Rigidbody::FetchPoseFromTransform ()
{
	AssertIf (m_Actor == NULL);
	Transform& transform = GetComponent (Transform);
	Vector3f pos = transform.GetPosition ();
	Quaternionf rot = transform.GetRotation ();
	NxMat33 nxrot ((const NxQuat&)rot);
	NxMat34 pose (nxrot, Vec3ToNx(pos));
	
	AssertFiniteParameter(pos)
	AssertFiniteParameter(rot)
	
	if (GetIsKinematic())
	{
		m_Actor->setGlobalPose (pose);
		// Novodex workaround ->Move global pose needs to always be called in order to make sure triggers get activated!
		m_Actor->moveGlobalPose (pose);
		m_DisableReadUpdateTransform = 1;
	}
	else
	{
		m_Actor->setGlobalPose (pose);
	}
}


void Rigidbody::TransformChanged (int mask)
{	
	if (m_Actor)
	{
		bool kine = GetIsKinematic();
		
		// When reading transform positions back from PhysX, we don't want to write back to PhysX again.
		// However, when a Rigidbody is kinematic or sleeping, and is moved because a parent rigidbody has changed,
		// we still want to update the position.
		if (GetPhysicsManager().IsRigidbodyTransformMessageEnabled() || kine || m_Actor->isSleeping())
		{
			Transform& transform = GetComponent (Transform);
			const int posRotMask = Transform::kRotationChanged | Transform::kPositionChanged;
			
			if ((mask & posRotMask) == posRotMask || (mask & Transform::kScaleChanged))
			{
				Vector3f pos = transform.GetPosition ();
				Quaternionf rot = transform.GetRotation ();
				NxMat33 nxrot ((const NxQuat&)rot);
				NxMat34 pose (nxrot, Vec3ToNx(pos));
				
				AssertFiniteParameter(pos)
				AssertFiniteParameter(rot)

				
				if (kine)
				{
					if ((mask & Transform::kAnimatePhysics) == 0)
						m_Actor->setGlobalPose (pose);
					// Novodex workaround ->Move global pose needs to always be called in order to make sure triggers get activated!
					m_Actor->moveGlobalPose (pose);
					m_DisableReadUpdateTransform = 1;
				}
				else
				{
					m_Actor->setGlobalPose (pose);
					if (m_InterpolationInfo)
						m_InterpolationInfo->disabled = 1;
				}
			}
			else if (mask & Transform::kRotationChanged)		
			{
				Quaternionf rot = transform.GetRotation ();
				AssertFiniteParameter(rot)
				if (kine)
				{
					if ((mask & Transform::kAnimatePhysics) == 0)
						m_Actor->setGlobalOrientationQuat ((const NxQuat&)rot);
					// Novodex workaround ->Move global pose needs to always be called in order to make sure triggers get activated!
					m_Actor->moveGlobalOrientationQuat ((const NxQuat&)rot);
					m_DisableReadUpdateTransform = 1;
				}
				else
				{
					m_Actor->setGlobalOrientationQuat ((const NxQuat&)rot);
								
					if (m_InterpolationInfo)
						m_InterpolationInfo->disabled = 1;
				}
			}
			else if (mask & Transform::kPositionChanged)
			{
				Vector3f pos = transform.GetPosition ();
				AssertFiniteParameter(pos)

				if (kine)
				{
					if ((mask & Transform::kAnimatePhysics) == 0)
						m_Actor->setGlobalPosition ((const NxVec3&)pos);
					// Novodex workaround ->Move global pose needs to always be called in order to make sure triggers get activated!
					m_Actor->moveGlobalPosition ((const NxVec3&)pos);
					m_DisableReadUpdateTransform = 1;
				}
				else
				{
					m_Actor->setGlobalPosition ((const NxVec3&)pos);
					if (m_InterpolationInfo)
						m_InterpolationInfo->disabled = 1;
				}
					
			}
		}
	}
	//printf_console ("Changed transform :%s\n", GetName ().c_str ());
}

void Rigidbody::ApplyRootMotionBuiltin (RootMotionData* rootMotion)
{	
	if (m_Actor == NULL || rootMotion->didApply)
		return;

	if(GetIsKinematic())
	{
		SetPosition(GetPosition() + rootMotion->deltaPosition);
		SetRotation(rootMotion->targetRotation);
	}
	else
	{
		Quaternionf rotation = GetRotation();
		Quaternionf invRotation = Inverse(rotation);
	
		// Get the physics velocity in local space
		Vector3f physicsVelocityLocal = RotateVectorByQuat(invRotation, GetVelocity());

		// Get the local space velocity and blend it with the physics velocity on the y-axis based on gravity weight
		// We do this in local space in order to support moving on a curve earth with gravity changing direction
		Vector3f animVelocityGlobal = rootMotion->deltaPosition * GetInvDeltaTime();
		Vector3f localVelocity = RotateVectorByQuat (invRotation, animVelocityGlobal);
		localVelocity.y = Lerp (localVelocity.y, physicsVelocityLocal.y, rootMotion->gravityWeight);
	
		// If we use gravity, when we are in a jumping root motion, we have to cancel out the gravity
		// applied by physX by default. When doing for example a jump the only thing affecting velocity should be the animation data.
		// The animation already has gravity applied in the animation data so to speak...
		if (GetUseGravity())
			AddForce(GetPhysicsManager().GetGravity() * -Lerp(1.0F, 0.0F, rootMotion->gravityWeight));
	
		// Apply velocity & rotation
		Vector3f globalVelocity = RotateVectorByQuat(rotation, localVelocity);
		SetVelocity(globalVelocity);
		MoveRotation(rootMotion->targetRotation);
	}	
	
	rootMotion->didApply = true;
}


Vector3f Rigidbody::GetPosition () const
{
	AssertIf (m_Actor == NULL);
	return Vec3FromNx (m_Actor->getGlobalPosition ());
}

Quaternionf Rigidbody::GetRotation () const
{
	AssertIf (m_Actor == NULL);
	return QuatFromNx(m_Actor->getGlobalOrientationQuat ());
}

void Rigidbody::SetPosition (const Vector3f& position)
{
	ABORT_INVALID_VECTOR3 (position, position, rigidbody);
	if (GetIsKinematic())
	{
		m_Actor->setGlobalPosition ((const NxVec3&)position);
		// Novodex workaround ->Move global pose needs to always be called in order to make sure triggers get activated!		
		m_Actor->moveGlobalPosition ((const NxVec3&)position);
		m_DisableReadUpdateTransform = 0;
	}
	else
	{
		if (m_InterpolationInfo)
			m_InterpolationInfo->disabled = 1;

		m_Actor->setGlobalPosition ((const NxVec3&)position);
	}
}

void Rigidbody::SetRotation (const Quaternionf& rotation)
{
	ABORT_INVALID_QUATERNION (rotation, rotation, rigidbody);

	if (GetIsKinematic())
	{
		m_Actor->setGlobalOrientationQuat ((const NxQuat&)rotation);
		// Novodex workaround ->Move global pose needs to always be called in order to make sure triggers get activated!		
		m_Actor->moveGlobalOrientationQuat ((const NxQuat&)rotation);
		m_DisableReadUpdateTransform = 0;
	}
	else
	{
		if (m_InterpolationInfo)
			m_InterpolationInfo->disabled = 1;

		m_Actor->setGlobalOrientationQuat ((const NxQuat&)rotation);
	}
}

void Rigidbody::MovePosition (const Vector3f& position)
{
	ABORT_INVALID_VECTOR3 (position, position, rigidbody);
	if (GetIsKinematic())
	{
		m_Actor->moveGlobalPosition ((const NxVec3&)position);
		m_DisableReadUpdateTransform = 0;
	}
	else
	{
		m_Actor->setGlobalPosition ((const NxVec3&)position);
	}
}

void Rigidbody::MoveRotation (const Quaternionf& rotation)
{
	ABORT_INVALID_QUATERNION (rotation, rotation, rigidbody);

	if (GetIsKinematic())
	{
		m_Actor->moveGlobalOrientation ((const NxQuat&)rotation);
		m_DisableReadUpdateTransform = 0;
	}
	else
	{
		m_Actor->setGlobalOrientation ((const NxQuat&)rotation);
	}
}

void Rigidbody::SetDensity (float density)
{
	if (m_Actor)
		m_Actor->updateMassFromShapes (density, 0.0F);
}

bool Rigidbody::IsSleeping ()
{
	return m_Actor->isSleeping();
}

void Rigidbody::Sleep ()
{
	return m_Actor->putToSleep();
}

void Rigidbody::WakeUp ()
{
	return m_Actor->wakeUp();
}


bool Rigidbody::GetDetectCollisions() const
{
	if (m_Actor)
		return !m_Actor->readActorFlag(NX_AF_DISABLE_COLLISION);
	else
		return true;
}

void Rigidbody::SetDetectCollisions(bool enable)
{
	if (m_Actor)
	{
		if (enable)
			m_Actor->clearActorFlag (NX_AF_DISABLE_COLLISION);	
		else
			m_Actor->raiseActorFlag (NX_AF_DISABLE_COLLISION);	
	}
}

bool Rigidbody::GetUseConeFriction() const
{
	if (m_Actor)
		return !m_Actor->readActorFlag(NX_AF_FORCE_CONE_FRICTION);
	else
		return true;
}

void Rigidbody::SetUseConeFriction(bool enable)
{
	if (m_Actor)
	{
		if (enable)
			m_Actor->clearActorFlag (NX_AF_FORCE_CONE_FRICTION);	
		else
			m_Actor->raiseActorFlag (NX_AF_FORCE_CONE_FRICTION);	
	}
}

void Rigidbody::UpdateInterpolationNode ()
{
	if (m_Interpolate == kNoInterpolation || !m_ActiveScene)
	{
		delete m_InterpolationInfo;
		m_InterpolationInfo = NULL;
	}
	else
	{
		if (m_InterpolationInfo == NULL)
		{
			m_InterpolationInfo = new RigidbodyInterpolationInfo();
			RigidbodyInterpolationInfo& info = *m_InterpolationInfo;
			info.body = this;
			info.disabled = 1;
			info.position = Vector3f::zero;
			info.rotation = Quaternionf::identity();
			GetPhysicsManager().GetInterpolatedBodies().push_back(*m_InterpolationInfo);
		}
	}
}

void Rigidbody::SetCollisionDetectionMode (int ccd)
{
	if (ccd != m_CachedCollisionDetection)
	{
		m_CollisionDetection = ccd;
		m_CachedCollisionDetection = ccd;
		
		if (m_Actor)
		{
			int shapeCount = m_Actor->getNbShapes ();
			NxShape*const * shapes = m_Actor->getShapes ();
			
			for (int i=0;i<shapeCount;i++)
			{
				Collider* collider = (Collider*)shapes[i]->userData;
				collider->ReCreate();
			}
		}
		
		SetDirty();
	}
}


bool Rigidbody::SweepTest (const Vector3f &direction, float distance, RaycastHit& outHit)
{
	AssertIf (!IsNormalized (direction));
	PROFILER_AUTO(gSweepTestProfile, NULL)
		
	if (m_Actor)
	{
		if (distance == std::numeric_limits<float>::infinity())
			// CapsuleCasts fail when using NX_MAX_F32 here.
			// So pick a lower "high" number instead.
			distance = 1000000.0f;
			
		NxSweepQueryHit hit;
		NxU32 nb = m_Actor->linearSweep ((const NxVec3&)direction * distance, NX_SF_DYNAMICS|NX_SF_STATICS, NULL, 1, &hit, NULL);
		if (nb)
		{
			NxToRaycastHit(hit, distance, outHit);
			return true;
		}
	}
	return false;
}

#define kSweepMaxHits 128
const PhysicsManager::RaycastHits& Rigidbody::SweepTestAll (const Vector3f &direction, float distance)
{
	AssertIf (!IsNormalized (direction));
	PROFILER_AUTO(gSweepTestAllProfile, NULL)
	
	if (distance == std::numeric_limits<float>::infinity())
		// CapsuleCasts fail when using NX_MAX_F32 here.
		// So pick a lower "high" number instead.
		distance = 1000000.0f;
	
	static vector<RaycastHit> outHits;

	if (m_Actor)
	{
		NxSweepQueryHit hits[kSweepMaxHits];
		NxU32 nb = m_Actor->linearSweep ((const NxVec3&)direction * distance, NX_SF_DYNAMICS|NX_SF_STATICS|NX_SF_ALL_HITS, NULL, kSweepMaxHits, hits, NULL);

		outHits.resize(nb);
		for (int i=0; i<nb; i++)
			NxToRaycastHit(hits[i], distance, outHits[i]);
	}
	return outHits;
}


IMPLEMENT_CLASS_HAS_INIT (Rigidbody)
IMPLEMENT_OBJECT_SERIALIZE (Rigidbody)
#endif // ENABLE_PHYSICS
