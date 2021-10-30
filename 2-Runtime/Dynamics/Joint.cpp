#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "Joint.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/AABBUtility.h"
#include "Runtime/Graphics/Transform.h"
#include "PhysicsManager.h"
#include "RigidBody.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "NxWrapperUtility.h"

namespace Unity
{

Joint::Joint (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_DidSetupAxes = false;
	m_Joint = NULL;
	m_Anchor = Vector3f::zero;
	m_AutoConfigureConnectedAnchor = true;
	m_ConnectedAnchor = Vector3f::zero;
	m_Axis = Vector3f::xAxis;

	m_BreakForce = std::numeric_limits<float>::infinity ();
	m_BreakTorque = std::numeric_limits<float>::infinity ();
}


Joint::~Joint ()
{
	Cleanup ();
}


void Joint::Reset ()
{
	// Anchor is at the edge of bounding volume .y !
	Super::Reset ();
	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
		m_Anchor = Vector3f (0, aabb.GetCenter ().y + aabb.GetExtent ().y, 0);
	else
		m_Anchor = Vector3f::zero;
	m_ConnectedAnchor = Vector3f::zero;
	m_AutoConfigureConnectedAnchor = true;
	m_Axis = Vector3f::xAxis;
}
	
void Joint::CheckConsistency ()	
{
	Super::CheckConsistency ();
	
	Rigidbody* body = QueryComponent (Rigidbody);	
	
	Rigidbody* otherBody = m_ConnectedBody;
	
	if (otherBody == body)
	{
		m_ConnectedBody = NULL;
	}
}

inline Vector3f GlobalToLocalUnscaledSpace (Transform& transform, Vector3f& p)
{
	Vector3f localAnchor = p - transform.GetPosition ();
	localAnchor = transform.InverseTransformDirection (localAnchor);
	return localAnchor;
}


/// OPTIMIZE ALL THOSE TRANSFORM ACCESSES.
/// Use a matrix instead.
void Joint::SetupAxes (NxJointDesc& desc, int options)
{
	Vector3f globalAnchor, globalAxis, globalNormal;
	CalculateGlobalHingeSpace (globalAnchor, globalAxis, globalNormal);
	Vector3f globalConnectedAnchor = CalculateGlobalConnectedAnchor (m_AutoConfigureConnectedAnchor);

	Transform& transform = GetComponent (Transform);
	Transform* otherTransform = NULL;
	Rigidbody* body;
	body = m_ConnectedBody;
	if (body)
		otherTransform = body->QueryComponent (Transform);

	// Setup anchor
	if (options & kChangeAnchor)
	{
		desc.localAnchor[0] = Vec3ToNx(GlobalToLocalUnscaledSpace (transform, globalAnchor));
	}
	if (options & kChangeAxis)
	{
		desc.localNormal[0] = Vec3ToNx(transform.InverseTransformDirection (globalNormal));
		desc.localAxis[0] = Vec3ToNx(transform.InverseTransformDirection (globalAxis));
	}
	
	if (otherTransform)
	{
		if (options & kChangeAnchor)
		{
			desc.localAnchor[1] = Vec3ToNx(GlobalToLocalUnscaledSpace (*otherTransform, globalConnectedAnchor));
		}
		if (options & kChangeAxis)
		{
			desc.localAxis[1] = Vec3ToNx(otherTransform->InverseTransformDirection (globalAxis));
			desc.localNormal[1] = Vec3ToNx(otherTransform->InverseTransformDirection (globalNormal));
		}
	}
	else
	{
		if (options & kChangeAnchor)
		{
			desc.localAnchor[1] = Vec3ToNx(globalConnectedAnchor);
		}
		if (options & kChangeAxis)
		{
			desc.localAxis[1] = Vec3ToNx(globalAxis);
			desc.localNormal[1] = Vec3ToNx(globalNormal);
		}
	}
}

Vector3f Joint::CalculateGlobalConnectedAnchor (bool autoConfigureConnectedFrame)
{
	Vector3f globalConnectedAnchor;
	Transform* otherTransform = NULL;
	Rigidbody* body = m_ConnectedBody;
	if (body)
		otherTransform = body->QueryComponent (Transform);
	
	if (autoConfigureConnectedFrame)
	{
		Vector3f globalAnchor = GetComponent (Transform).TransformPoint (m_Anchor);
		if (otherTransform)
			m_ConnectedAnchor = otherTransform->InverseTransformPoint (globalAnchor);
		else
			m_ConnectedAnchor = globalAnchor;
	}
	
	if (otherTransform)
		globalConnectedAnchor = otherTransform->TransformPoint (m_ConnectedAnchor);
	else
		globalConnectedAnchor = m_ConnectedAnchor;
	
	return globalConnectedAnchor;
}

void Joint::CalculateGlobalHingeSpace (Vector3f& globalAnchor, Vector3f& globalAxis, Vector3f& globalNormal) const
{
	const Transform& transform = GetComponent (Transform);

	Vector3f localAxis = m_Axis;
	if (SqrMagnitude (localAxis) < Vector3f::epsilon)
		localAxis = Vector3f (1.0F, 0.0F, 0.0F);

	globalAnchor = transform.TransformPoint (m_Anchor);

	Vector3f localDirection = - m_Anchor;
	Vector3f localNormal = Cross (localDirection, localAxis);
	OrthoNormalize (&localAxis, &localNormal);
	
	globalAxis = transform.TransformDirection (localAxis);
	globalNormal = transform.TransformDirection (localNormal);
}

float ToNovodexInfinity (float f)
{
	if (f == std::numeric_limits<float>::infinity ())
		return NX_MAX_REAL;
	else
		return f;
}

void Joint::SetBreakForce (float force)
{
	SetDirty ();
	m_BreakForce = force;
	if (m_Joint)
		m_Joint->setBreakable (ToNovodexInfinity (m_BreakForce), GetBreakTorque ());
}

void Joint::SetBreakTorque (float torque)
{
	SetDirty ();
	m_BreakTorque = torque;
	if (m_Joint)
		m_Joint->setBreakable (GetBreakForce (), ToNovodexInfinity (m_BreakTorque));
}

float Joint::GetBreakForce () const
{
	return m_BreakForce;
}

float Joint::GetBreakTorque () const
{
	return m_BreakTorque;
}

void Joint::FinalizeCreateImpl (NxJointDesc& desc, bool swapActors)
{
	desc.maxForce = ToNovodexInfinity (m_BreakForce);
	desc.maxTorque = ToNovodexInfinity (m_BreakTorque);
	desc.userData = this;
		
	Rigidbody& body = GetComponent (Rigidbody);
	body.Create (true);
	body.FetchPoseFromTransform ();

	AssertIf (!body.m_ActiveScene);
	bool didActorsChange = false;
	
	int actor0 = swapActors ? 1 : 0;
	int actor1 = swapActors ? 0 : 1;

	if (desc.actor[actor0] != body.m_Actor)
	{
		desc.actor[actor0] = body.m_Actor;
		didActorsChange = true;
	}
			
	Rigidbody* otherBody = m_ConnectedBody;
	if (otherBody != NULL && otherBody->IsActive ())
	{
		otherBody->Create (true);
		otherBody->FetchPoseFromTransform ();

		AssertIf (!otherBody->m_ActiveScene);
		if (desc.actor[actor1] != otherBody->m_Actor)
		{
			desc.actor[actor1] = otherBody->m_Actor;
			didActorsChange = true;
		}
	}
	else
	{
		if (desc.actor[actor1] != NULL)
		{
			desc.actor[actor1] = NULL;
			didActorsChange = true;
		}
	}
	
	// If we don't recreate the joint novodex will mark the joint broken and then we get an assert.
	// because we can't save to the desc anymore!
	if (didActorsChange)
		Cleanup ();
	
	if (!m_DidSetupAxes || m_Joint == NULL)
	{
		SetupAxes (desc);
		m_DidSetupAxes = true;
	}
}

void Joint::Cleanup ()
{
	if (m_Joint)
	{
		GetDynamicsScene ().releaseJoint (*m_Joint);
		m_Joint = NULL;
	}
}

void Joint::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	if (IsActive ())
		Create ();
	else
		Cleanup ();
}

void Joint::Deactivate (DeactivateOperation operation)
{
	Cleanup ();
	Super::Deactivate (operation);
}

void Joint::SetConnectedBody (PPtr<Rigidbody> body)
{
	if (m_ConnectedBody != body)
	{
		SetDirty ();
		m_ConnectedBody = body;
	}
	
	if (IsActive ())
		Create ();
}

void Joint::SetAxis (const Vector3f& axis)
{
	SetDirty ();
	m_Axis = axis;
	ApplySetupAxesToDesc(kChangeAxis);
}
		
void Joint::SetAnchor (const Vector3f& anchor)
{
	SetDirty ();
	m_Anchor = anchor;
	ApplySetupAxesToDesc(kChangeAnchor);
}
	
void Joint::SetConnectedAnchor (const Vector3f& anchor)
{
	SetDirty ();
	m_ConnectedAnchor = anchor;
	ApplySetupAxesToDesc(kChangeAnchor);
}
	
void Joint::SetAutoConfigureConnectedAnchor (bool anchor)
{
	SetDirty ();
	m_AutoConfigureConnectedAnchor = anchor;
	ApplySetupAxesToDesc(kChangeAxis|kChangeAnchor);
}

}

IMPLEMENT_CLASS (Joint)

#endif //ENABLE_PHYSICS