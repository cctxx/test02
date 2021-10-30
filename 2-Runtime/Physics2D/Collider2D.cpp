#include "UnityPrefix.h"

#if ENABLE_2D_PHYSICS
#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Physics2D/RigidBody2D.h"
#include "Runtime/Physics2D/Physics2DManager.h"
#include "Runtime/Physics2D/Physics2DSettings.h"
#include "Runtime/Physics2D/Physics2DMaterial.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Utilities/Utility.h"


PROFILER_INFORMATION(gPhysics2DProfileColliderCleanup, "Physics2D.ColliderCleanup", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileColliderShapeGeneration, "Physics2D.ColliderShapeGeneration", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileColliderTransformChanged, "Physics2D.ColliderTransformChanged", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileColliderTransformParentChanged, "Physics2D.ColliderTransformParentChanged", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileColliderTransformScaleChanged, "Physics2D.ColliderTransformScaleChanged", kProfilerPhysics)
PROFILER_INFORMATION(gPhysics2DProfileColliderTransformPositionRotationChanged, "Physics2D.ColliderTransformPositionRotationChanged", kProfilerPhysics)


IMPLEMENT_CLASS_HAS_INIT (Collider2D)
IMPLEMENT_OBJECT_SERIALIZE (Collider2D)
INSTANTIATE_TEMPLATE_TRANSFER (Collider2D)

// --------------------------------------------------------------------------


Collider2D::Collider2D (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_IsTrigger(false)
,	m_IsStaticBody(false)
,	m_LocalTransformInitialized(false)
{
}


Collider2D::~Collider2D ()
{
	Cleanup ();
}


void Collider2D::InitializeClass ()
{
	REGISTER_MESSAGE (Collider2D, kTransformChanged, TransformChanged, int);
}


template<class TransferFunction>
void Collider2D::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);

	TRANSFER(m_Material);
	transfer.Transfer (m_IsTrigger, "m_IsTrigger");
	transfer.Align();
}


void Collider2D::Reset ()
{
	Super::Reset ();

	m_Material = 0;
	m_IsTrigger = false;
}


void Collider2D::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	// Ignore if not active.
	if (!IsActive())
		return;

	// (Re)create collider if appropriate.
	Create ();
}


void Collider2D::Deactivate (DeactivateOperation operation)
{
	Super::Deactivate (operation);
	Cleanup ();

	// Destroy collider collisions immediately.
	GetPhysics2DManager().DestroyColliderCollisions (this);
}


void Collider2D::Cleanup ()
{
	PROFILER_AUTO(gPhysics2DProfileColliderCleanup, NULL)

	// Process any shapes.
	if (m_Shapes.size() > 0)
	{
		if (m_IsStaticBody)
		{
			// Destroy the fixtures
			b2Body* groundBody = GetPhysicsGroundBody ();
			for (int i = 0; i < m_Shapes.size(); ++i)
				groundBody->DestroyFixture(m_Shapes[i]);
		}
		else
		{
			Rigidbody2D* rigidBody = GetRigidbody();
			if (rigidBody)
			{
				// Destroy the fixtures.
				b2Body* body = rigidBody->GetBody ();
				for (int i = 0; i < m_Shapes.size(); ++i)
					body->DestroyFixture(m_Shapes[i]);

				// Recalculate the collider body-mass.
				rigidBody->CalculateColliderBodyMass ();
			}
		}

		m_Shapes.clear();

		// Invalidate any persisted contact information.
		GetPhysics2DManager().InvalidateColliderCollisions (this);
	}

	m_IsStaticBody = false;
}


void Collider2D::RecreateCollider (const Rigidbody2D* ignoreRigidbody)
{
	if (IsActive () && GetEnabled())
		Create (ignoreRigidbody);
	else
		Cleanup ();
}


void Collider2D::AddToManager ()
{
	// Create the collider if not already created.
	if (m_Shapes.size() == 0)
		Create ();
}


void Collider2D::RemoveFromManager ()
{
	// Destroy the collider.
	Cleanup ();
}


void Collider2D::SetIsTrigger (bool trigger)
{
	// Finish if no change.
	if (m_IsTrigger == trigger)
		return;

	m_IsTrigger = trigger;

	SetDirty();

	if (IsActive () && GetEnabled())
		Create ();
}


PPtr<PhysicsMaterial2D> Collider2D::GetMaterial ()
{
	return m_Material;
}


void Collider2D::SetMaterial (PPtr<PhysicsMaterial2D> material)
{
	if (m_Material == material)
		return;

	SetDirty ();
	m_Material = material;
}


Rigidbody2D* Collider2D::GetRigidbody ()
{
	// Finish if a static body or no shapes.
	if (m_IsStaticBody || m_Shapes.size() == 0)
		return NULL;

	// Fetch body from first shape.
	b2Body* body = m_Shapes[0]->GetBody();

	if (!body)
		return NULL;

	return (Rigidbody2D*)body->GetUserData();
}


bool Collider2D::OverlapPoint (const Vector2f& point) const
{
	// Cannot perform query if not active.
	if (!IsActive ())
		return false;

	const b2Vec2 testPoint(point.x, point.y);

	// Test all fixtures for the point.
	for (FixtureArray::const_iterator fixtureItr = m_Shapes.begin (); fixtureItr != m_Shapes.end (); ++fixtureItr)
	{
		if ((*fixtureItr)->TestPoint (testPoint))
			return true;
	}

	return false;
}


void Collider2D::TransformChanged (int changeMask)
{
	PROFILER_AUTO(gPhysics2DProfileColliderTransformChanged, NULL)

	// Finish if transform message is disabled.
	if (!GetPhysics2DManager().IsTransformMessageEnabled())
		return;

	// Recreate the collider if the parent body has changed.
	if (changeMask & Transform::kParentingChanged)
	{
		PROFILER_AUTO(gPhysics2DProfileColliderTransformParentChanged, NULL)

		Rigidbody2D* currentBody = GetRigidbody();
		Rigidbody2D* newBody = Rigidbody2D::FindRigidbody (GetGameObjectPtr());
		if (newBody != currentBody)
		{
			Create();
			return;
		}
	}

	// Recreate the collider if the scale has changed.
	if (changeMask & Transform::kScaleChanged)
	{
		PROFILER_AUTO(gPhysics2DProfileColliderTransformScaleChanged, NULL)

		Create();
		return;
	}

	// Recreate the collider if the position or rotation has changed.
	if (changeMask & (Transform::kPositionChanged | Transform::kRotationChanged))
	{
		PROFILER_AUTO(gPhysics2DProfileColliderTransformPositionRotationChanged, NULL)

		// Always recreate static bodies but only recreate non-static bodies if the rigid-body exists
		// on a parent GameObject i.e. this is a compound object.
		if (!m_IsStaticBody && (QueryComponent (Rigidbody2D) != NULL || !HasLocalTransformChanged ()))
			return;

		Create();
	}
}


// --------------------------------------------------------------------------


void Collider2D::FinalizeCreate(b2FixtureDef& def, b2Body* body, const dynamic_array<b2Shape*>* shapes)
{
	Assert (m_Shapes.size() == 0);

	PROFILER_AUTO(gPhysics2DProfileColliderShapeGeneration, NULL)

	// Don't create the collider if not active and enabled.
	if (!(IsActive () && GetEnabled ()))
		return;

	// Initialize fixture definition.
	if (m_Material.IsNull ())
	{
		PhysicsMaterial2D* defaultMaterial = GetPhysics2DSettings ().GetDefaultPhysicsMaterial ();
		if (defaultMaterial != NULL )
		{
			def.friction = defaultMaterial->GetFriction ();
			def.restitution = defaultMaterial->GetBounciness ();
		}
		else
		{
			def.friction = 0.4f;
			def.restitution = 0.0f;
		}
	}
	else
	{
		def.friction = m_Material->GetFriction();
		def.restitution = m_Material->GetBounciness();
	}
	def.density = 1.0f;	// Body mass is calculated based upon a mass calculation of each shape unit-density.
	def.userData = this;
	def.isSensor = m_IsTrigger;	

	// Fetch the associated rigid-body.
	// NOTE: There won't be one if the body is the ground-body!
	Rigidbody2D* rigidBody = (Rigidbody2D*)body->GetUserData ();

	// Do we have any shapes?
	if (shapes)
	{
		// Yes, so add shape to existing set.
		const int shapeCount = shapes->size();

		Assert(shapeCount > 0);
		Assert(def.shape == 0);

		m_Shapes.resize_uninitialized(shapeCount);
		for (int i = 0; i < shapeCount; ++i)
		{
			b2FixtureDef prototype = def;
			prototype.shape = (*shapes)[i];
			m_Shapes[i] = body->CreateFixture(&prototype);
		}
	}
	else
	{
		// No, so add new shape to set.
		m_Shapes.resize_uninitialized(1);
		m_Shapes[0] = body->CreateFixture(&def);
	}

	// Calculate the collider body mass.
	if (rigidBody != NULL)
		rigidBody->CalculateColliderBodyMass ();

	// Update the local transform.
	UpdateLocalTransform ();
}


void Collider2D::CalculateColliderTransformation (const Rigidbody2D* ignoreRigidbody, b2Body** attachedBody, Matrix4x4f& matrix)
{
	// Fetch the transform.
	const Transform& transform = GetComponent(Transform);

	// Do we have a rigid-body on the same game object?
	Rigidbody2D* rigidBody = QueryComponent (Rigidbody2D);
	if (rigidBody && rigidBody != ignoreRigidbody && rigidBody->IsActive () && rigidBody->GetBody () != NULL)
	{
		// Yes, so no transformation.
		matrix.SetIdentity();

		// Ensure the rigid-body is available.
		Assert (rigidBody->GetBody() != NULL);

		// Set attached body.
		*attachedBody = rigidBody->GetBody();

		// Flag as non-static.
		m_IsStaticBody = false;

		return;
	}

	// Find valid rigid-body parent transform.
	Transform* parentTransform = transform.GetParent ();

	while(parentTransform)
	{
		// Fetch the grandparent transform.
		Transform* grandParentTransform = parentTransform->GetParent ();

		// Fetch the parent game object.
		GameObject* go = parentTransform->GetGameObjectPtr ();
		if (go)
		{
			// Fetch any rigid-body.
			rigidBody = go->QueryComponent (Rigidbody2D);

			// Is the rigid-body valid or we're at the transform root?
			if (rigidBody && rigidBody != ignoreRigidbody && rigidBody->IsActive () && rigidBody->GetBody () != NULL)
			{
				// Yes, so calculate its relative transformation.
				Vector3f childPosition = transform.GetPosition ();
				Quaternionf childRotation = transform.GetRotation ();
				Matrix4x4f childMatrix, parentMatrix;	
				childMatrix.SetTR (childPosition, childRotation);
				parentMatrix = parentTransform->GetWorldToLocalMatrixNoScale ();
				MultiplyMatrices4x4 (&parentMatrix, &childMatrix, &matrix);

				// Ensure the rigid-body is available.
				Assert (rigidBody->GetBody() != NULL);

				// Set attached body.
				*attachedBody = rigidBody->GetBody();

				// Flag as non-static.
				m_IsStaticBody = false;

				return;
			}
		}

		// Move up hierarchy.
		parentTransform = grandParentTransform;
	}

	// Fetch the transformation from the origin (ground-body position).
	matrix = transform.GetLocalToWorldMatrixNoScale();

	// Set attached body (ground-body).
	*attachedBody = GetPhysicsGroundBody();
	
	// Flag as static.
	m_IsStaticBody = true;
}


bool Collider2D::HasLocalTransformChanged () const
{
	// Fetch the transform.
	const Transform& transform = GetComponent(Transform);

	return !m_LocalTransformInitialized || m_LocalPosition != transform.GetLocalPosition () ||	m_LocalRotation != transform.GetLocalRotation ();
}


void Collider2D::UpdateLocalTransform ()
{
	// Fetch the transform.
	const Transform& transform = GetComponent(Transform);

	m_LocalPosition = transform.GetLocalPosition ();
	m_LocalRotation = transform.GetLocalRotation ();
	m_LocalTransformInitialized = true;
}


#endif // #if ENABLE_2D_PHYSICS
