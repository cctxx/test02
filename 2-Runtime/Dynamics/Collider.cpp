#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Collider.h"
#include "RigidBody.h"
#include "Runtime/Graphics/Transform.h"
#include "PhysicMaterial.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "PhysicsManager.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/Scripting.h"

/* Problems and limitations

- When a rigid body is added or activated after the colliders are setup it won't attach them automatically

- When a rigid body is destroyed it will recreate all its colliders as colliders without rigidbody. (Wasteful)
  We should have activity state be treated hierarchically and deactivating/destroying children before parents.

- Implement automatic instantiation when accessing a material

- Colliders should be enlarged by min penetration epsilon. This way we prevent interpentration completely.

- Get rigidbody only works when the rigidbody is active!

*/

#if ENABLE_PROFILER
ProfilerInformation gStaticColliderModify ("Static Collider.Modify (Expensive delayed cost)", kProfilerPhysics, true);
ProfilerInformation gStaticColliderMove ("Static Collider.Move (Expensive delayed cost)", kProfilerPhysics, true);
ProfilerInformation gStaticColliderCreate ("Static Collider.Create (Expensive delayed cost)", kProfilerPhysics, true);
ProfilerInformation gDynamicColliderCreate ("Dynamic Collider.Create", kProfilerRender);
#endif

Collider::Collider (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Shape = NULL;
	m_IsTrigger = false;
	m_Enabled = true;
}

Collider::~Collider ()
{
	Cleanup ();
}

void Collider::SetEnabled (bool enab)
{
	if ((bool)m_Enabled == enab)
		return;
	m_Enabled = enab;
	Cleanup ();
	CreateShapeIfNeeded ();
	SetDirty ();
}

void Collider::RecreateCollider (const Rigidbody* ignoreCollider)
{
	AssertIf (GetClassID() == 143);// Charactercontroller
	
	if (IsActive () && GetEnabled())
		Create (ignoreCollider);
}

Rigidbody* Collider::GetRigidbody ()
{
	if (m_Shape)
	{
		Rigidbody* body = (Rigidbody*)m_Shape->getActor ().userData;
		return body;
	}
	else
		return NULL;
}

void Collider::Deactivate (DeactivateOperation operation)
{
	Super::Deactivate (operation);
	Cleanup ();
}

NxShape* Collider::CreateShapeIfNeeded()
{
	if (m_Shape != NULL)
		return m_Shape;
	if (IsActive () && GetEnabled())
		Create(NULL);
	
	return m_Shape;
}

void Collider::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	
	if (IsActive () && GetEnabled())
	{
		if (m_Shape)
		{
			if (SupportsMaterial())
				SetMaterial (m_Material);
			SetIsTrigger (m_IsTrigger);
		}
		CreateShapeIfNeeded();
	}
	else
		Cleanup ();
}

bool Collider::HasActorRigidbody ()
{
	return m_Shape && m_Shape->getActor ().userData;
}

bool Collider::GetRelativeToParentPositionAndRotation (Transform& transform, Transform& anyParent, Matrix4x4f& matrix)
{
	if (&transform == &anyParent)
	{
		matrix.SetIdentity ();
		return true;
	}
	else
	{
		Vector3f childPosition = transform.GetPosition ();
		Quaternionf childRotation = transform.GetRotation ();

		Matrix4x4f childMatrix, parentMatrix;
		
		childMatrix.SetTR (childPosition, childRotation);
		parentMatrix = anyParent.GetWorldToLocalMatrixNoScale ();
		
		MultiplyMatrices4x4 (&parentMatrix, &childMatrix, &matrix);
		ErrorFiniteParameterReturnFalse(matrix)
		return true;
	}
}

bool Collider::GetRelativeToParentPositionAndRotationUtility (Transform& transform, Transform& anyParent, const Vector3f& localOffset, Matrix4x4f& matrix)
{
	Vector3f childPosition = transform.TransformPoint (localOffset);
	Quaternionf childRotation = transform.GetRotation ();
			
	Matrix4x4f childMatrix, parentMatrix;
	
	childMatrix.SetTR (childPosition, childRotation);
	parentMatrix = anyParent.GetWorldToLocalMatrixNoScale ();
	
	MultiplyMatrices4x4 (&parentMatrix, &childMatrix, &matrix);
	ErrorFiniteParameterReturnFalse(matrix)
	return true;
}

void Collider::FetchPoseFromTransformUtility (const Vector3f& offset)
{
	AssertIf (m_Shape == NULL);
	AssertIf (HasActorRigidbody ());
	Transform& transform = GetComponent (Transform);

	Vector3f pos = transform.TransformPoint (offset);
	Quaternionf rot = transform.GetRotation ();

	AssertFiniteParameter(pos)
	AssertFiniteParameter(rot)

	NxMat34 shapeMatrix ((const NxQuat&)rot, (const NxVec3&)pos);
	m_Shape->setGlobalPose(shapeMatrix);
}

void Collider::FetchPoseFromTransform ()
{
	AssertIf (m_Shape == NULL);
	AssertIf (HasActorRigidbody ());
	Transform& transform = GetComponent (Transform);

	Quaternionf rot; Vector3f pos;
	transform.GetPositionAndRotation(pos, rot);

	AssertFiniteParameter(pos)
	AssertFiniteParameter(rot)

	NxMat34 shapeMatrix ((const NxQuat&)rot, (const NxVec3&)pos);
	m_Shape->setGlobalPose(shapeMatrix);
}

void Collider::RigidbodyMassDistributionChanged ()
{
	if (m_Shape)
	{
		Rigidbody* body = (Rigidbody*)m_Shape->getActor ().userData;
		if (body)
			body->UpdateMassDistribution();
	}
}

void Collider::CreateWithoutIgnoreAttach () 
{ 	
	if ((IsActive () && GetEnabled()) || !IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
		Create(NULL); 
}

NxCCDSkeleton* Collider::CreateCCDSkeleton()
{
	float kCCDScale = 0.8f;
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_2_a1))
		kCCDScale = 0.5f;
	return CreateCCDSkeleton(kCCDScale);
}

void Collider::UpdateCCDSkeleton()
{
	if (m_Shape)
	{
		Rigidbody* body = (Rigidbody*)m_Shape->getActor ().userData;
		if (body && body->GetCollisionDetectionMode() != Rigidbody::kCCDModeOff)
		{
			NxCCDSkeleton *skel = m_Shape->getCCDSkeleton();
			
			m_Shape->setCCDSkeleton(CreateCCDSkeleton());
			
			if (skel)
				GetDynamicsSDK().releaseCCDSkeleton (*skel);
		}
	}
}


void Collider::FinalizeCreate( NxShapeDesc& shapeDesc, bool setMaterial, const Rigidbody* dontAttachToRigidbody )
{
	AssertIf (GetClassID() == 143);// Charactercontroller
	
	AssertIf (m_Shape != NULL);
	if (m_IsTrigger)
	{
		shapeDesc.shapeFlags |= NX_TRIGGER_ENABLE;
		if (!GetPhysicsManager().GetRaycastsHitTriggers())
			shapeDesc.shapeFlags |= NX_SF_DISABLE_RAYCASTING;
	}

	Rigidbody* body = FindNewAttachedRigidbody (dontAttachToRigidbody);
		
	shapeDesc.userData = this;
	// if !setMaterial is passed, the caller has the material set up (e.g. WheelCollider doesn't want it)
	if( setMaterial )
	{
		shapeDesc.materialIndex = GetMaterialIndex ();
	}
	shapeDesc.group = GetGameObject ().GetLayer ();
	
	if (!shapeDesc.isValid())
	{
		ErrorStringObject ("This collider has some illegal parameters. Choose 'Reset' in the component popup menu to fix it.", this);
		return;
	}
	
	if (body)
	{
		PROFILER_AUTO(gDynamicColliderCreate, this)

		if (body->GetCollisionDetectionMode() != Rigidbody::kCCDModeOff)
		{
			if (body->GetCollisionDetectionMode() == Rigidbody::kCCDModeDynamic)
				shapeDesc.shapeFlags |= NX_SF_DYNAMIC_DYNAMIC_CCD;
				
			// CCD skeletons should be smaller then the normal colliders so that collisions at normal speeds can 
			// be handled by those.
			NxCCDSkeleton *skel = CreateCCDSkeleton();
			shapeDesc.ccdSkeleton = skel;
		}

		body->Create (true);
		Matrix4x4f matrix;
		NxActor* actor = body->m_Actor;
		if (actor == NULL)
		{
			ErrorStringObject ("Could not create actor. Maybe you are using too many colliders or rigidbodies in your scene?", this);
			return;
		}
		
		if (GetRelativeToParentPositionAndRotation (GetComponent (Transform), body->GetComponent (Transform), matrix))
		{
			shapeDesc.localPose.setColumnMajor44 (matrix.GetPtr ());
			
			m_Shape = actor->createShape (shapeDesc);
		}

		if (!m_IsTrigger && GetClassID()!= 140)
			actor->updateMassFromShapes (0.0F, body->GetMass ());
	}
	else
	{
		PROFILER_AUTO(gStaticColliderCreate, this)

		NxActorDesc actorDesc;
		actorDesc.userData = NULL;
		actorDesc.shapes.push_back (&shapeDesc);
		NxActor* actor = GetDynamicsScene ().createActor (actorDesc);
		if (actor == NULL)
		{
			ErrorStringObject ("Could not create actor. Maybe you are using too many colliders or rigidbodies in your scene?", this);
			return;
		}

		m_Shape = actor->getShapes ()[0];
		FetchPoseFromTransform ();

		SupportedMessagesDidChange (GetGameObject ().GetSupportedMessages ());
	}
}

void Collider::SupportedMessagesDidChange (int supported)
{
	// We only deal with colliders with no rigid body attached
	if (m_Shape == NULL || m_Shape->getActor ().userData != NULL)
		return;
		
	if (supported & kHasCollisionStay)
		m_Shape->getActor ().setGroup (kContactTouchGroup);
	else if (supported & (kHasCollisionStay | kHasCollisionEnterExit))
		m_Shape->getActor ().setGroup (kContactEnterExitGroup);
	else
		m_Shape->getActor ().setGroup (kContactNothingGroup);
}

void Collider::SetupLayer ()
{
	if (m_Shape)
		m_Shape->setGroup (GetGameObject ().GetLayer ());
}

Rigidbody* Collider::FindNewAttachedRigidbody (const Rigidbody* ignoreAttachRigidbody)
{
	Rigidbody* body = QueryComponent (Rigidbody);
	if (body && body->IsActive () && body != ignoreAttachRigidbody)
		return body;

	Transform* parent = GetComponent (Transform).GetParent ();
	while (parent)
	{
		GameObject* go = parent->GetGameObjectPtr ();
		if (go)
			body = go->QueryComponent (Rigidbody);
		else
			body = NULL;
		if (body && body->IsActive () && body != ignoreAttachRigidbody)
			return body;
		
		parent = parent->GetParent ();
	}
	return NULL;
}

int Collider::GetMaterialIndex ()
{
	PhysicMaterial* material = m_Material;
	if (material)
		return material->GetMaterialIndex ();
	else
		return 0;
}

void Collider::SetIsTrigger (bool trigger)
{
	if (m_IsTrigger != trigger)
	{
		SetDirty ();
		m_IsTrigger = trigger;
	}
	
	if (m_Shape)
	{
		m_Shape->setFlag (NX_TRIGGER_ENABLE, trigger);
		m_Shape->setFlag (NX_SF_DISABLE_RAYCASTING, trigger && !GetPhysicsManager().GetRaycastsHitTriggers());

		RigidbodyMassDistributionChanged ();
	}
}

AABB Collider::GetBounds ()
{
	if (m_Shape)
	{
		AABB aabb;
		NxBounds3 bounds;
		m_Shape->getWorldBounds(bounds);
		bounds.getExtents ((NxVec3&)aabb.GetExtent ());
		bounds.getCenter ((NxVec3&)aabb.GetCenter ());
		return aabb;
	}
	else
	{
		return AABB (GetComponent (Transform).GetPosition (), Vector3f::zero);
	}
}

void Collider::Cleanup ()
{
	if (m_Shape)
	{
		AssertIf (GetClassID() == 143);
		
		NxCCDSkeleton *skel = m_Shape->getCCDSkeleton();
			
		if (m_Shape->getActor ().userData)
			m_Shape->getActor ().releaseShape (*m_Shape);
		else
			GetDynamicsScene ().releaseActor (m_Shape->getActor ());

		// Need to release skeleton after the actor  or shape using it is release by physics, 
		// so we don't get an error about releasing a ccd mesh which is in use.
		if (skel)
			GetDynamicsSDK().releaseCCDSkeleton (*skel);

		m_Shape = NULL;
	}
}

void Collider::ReCreate()
{
	if( !m_Shape )
		return;
	Cleanup();
	Create(NULL);
}

PPtr<PhysicMaterial> Collider::GetMaterial ()
{
	return m_Material;
}

void Collider::SetMaterial (PPtr<PhysicMaterial> material)
{
	if (!SupportsMaterial())
		ErrorStringObject ("Setting the Material property is not supported for Colliders of type " +  GetClassName() + ".", this);
		
	if (m_Material != material)
	{
		SetDirty ();
		m_Material = material;
	}
	
	if (m_Shape)
		m_Shape->setMaterial (GetMaterialIndex ());
}

void Collider::TransformChanged (int changeMask)
{
	if (m_Shape)
	{
		if (changeMask & Transform::kParentingChanged)
		{
			if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_4_a1))
			{
				// Only recreate if the rigidbody attachement is actually changed by this.
				// Otherwise we might unnecessarily cause trigger state to reset.
				Rigidbody* body = (Rigidbody*)m_Shape->getActor ().userData;
				Rigidbody* newBody = FindNewAttachedRigidbody (NULL);
				if (newBody != body)
					ReCreate();
			}
			else if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_2_a1))
				ReCreate();
		}
	}
}

void Collider::ClosestPointOnBounds (const Vector3f& position, Vector3f& outPosition, float& outSqrDistance)
{
	outSqrDistance = std::numeric_limits<float>::infinity();

	if (m_Shape)
	{
		NxBounds3 bounds;
		m_Shape->getWorldBounds(bounds);
		AABB aabb;
		bounds.getCenter((NxVec3&)aabb.GetCenter());
		bounds.getExtents((NxVec3&)aabb.GetExtent());
		
		CalculateClosestPoint(position, aabb, outPosition, outSqrDistance);
	}
	else
	{
		outPosition = GetComponent(Transform).GetPosition();
		outSqrDistance = SqrMagnitude(position - outPosition);
	}
}

bool Collider::Raycast (const Ray& ray, float distance, RaycastHit& outHit)
{
	AssertIf (!IsNormalized (ray.GetDirection ()));
	
	if (distance == std::numeric_limits<float>::infinity())
		distance = NX_MAX_F32;
	
	NxRaycastHit hit;
	if (m_Shape && m_Shape->raycast ((NxRay&)ray, distance, 0xffffffff, hit, false))
	{
		NxToRaycastHit(hit, outHit);
		return true;
	}
	else
		return false;
}

#if UNITY_EDITOR
void Collider::RefreshPhysicsInEditMode()
{
	if ( !IsWorldPlaying() )
	{
		GetPhysicsManager().RefreshWhenPaused();
	}
}
#endif

template<class TransferFunction>
void Collider::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);

	if (SupportsMaterial())
		TRANSFER_SIMPLE (m_Material);

	TRANSFER_SIMPLE (m_IsTrigger);
	transfer.Transfer (m_Enabled, "m_Enabled", kHideInEditorMask | kEditorDisplaysCheckBoxMask);
	transfer.Align();
}

void Collider::InitializeClass ()
{
	REGISTER_MESSAGE_VOID (Collider, kLayerChanged, SetupLayer);
	REGISTER_MESSAGE_VOID (Collider, kForceRecreateCollider, CreateWithoutIgnoreAttach);
	REGISTER_MESSAGE (Collider, kTransformChanged, TransformChanged, int);
}


#if ENABLE_SCRIPTING
ScriptingObjectPtr ConvertContactToMono (Collision* input)
{
        Collision& contact = *reinterpret_cast<Collision*> (input);
        MonoCollision monoContact;
        if (contact.flipped)
        {
                monoContact.rigidbody = Scripting::ScriptingWrapperFor (contact.thisRigidbody);
                monoContact.collider = Scripting::ScriptingWrapperFor (contact.thisCollider);
                monoContact.relativeVelocity = contact.relativeVelocity;
        }
        else
        {
                monoContact.rigidbody = Scripting::ScriptingWrapperFor (contact.otherRigidbody);
                monoContact.collider = Scripting::ScriptingWrapperFor (contact.otherCollider);
                monoContact.relativeVelocity = -contact.relativeVelocity;
        }
        ScriptingArrayPtr contacts = CreateScriptingArray<MonoContactPoint>(GetMonoManager ().GetCommonClasses ().contactPoint,contact.contacts.size());
        monoContact.contacts = contacts;
        int j = 0;
        for (Collision::Contacts::iterator i=contact.contacts.begin ();i != contact.contacts.end ();i++)
        {
				#if UNITY_WINRT
					MonoContactPoint contactPoint;
				#else
					MonoContactPoint& contactPoint = Scripting::GetScriptingArrayElement<MonoContactPoint> (contacts, j);
				#endif
                contactPoint.point = i->point;
                if (contact.flipped)
                {
                        contactPoint.thisCollider = Scripting::ScriptingWrapperFor (i->collider[1]);
                        contactPoint.otherCollider = Scripting::ScriptingWrapperFor (i->collider[0]);
                        contactPoint.normal = -i->normal;
                }
                else
                {
                        contactPoint.thisCollider = Scripting::ScriptingWrapperFor (i->collider[0]);
                        contactPoint.otherCollider = Scripting::ScriptingWrapperFor (i->collider[1]);
                        contactPoint.normal = i->normal;
                }
				#if UNITY_WINRT
					// A slower way to set a value in the array:
					//  * we create a scripting object;
					//  * then marshal data from contactPoint to that scripting object
					//  * and only then we're setting it in the array
					// At the moment there's no other way, unless we remove all ScriptingObjectPtr from MonoContactPoint
					Scripting::SetScriptingArrayElement(contacts, j, CreateScriptingObjectFromNativeStruct<MonoContactPoint>(GetMonoManager ().GetCommonClasses ().contactPoint, contactPoint));
				#endif
                j++;
        }
        return CreateScriptingObjectFromNativeStruct<MonoCollision>(GetMonoManager ().GetCommonClasses ().collision, monoContact);
}
#endif //ENABLE_SCRIPTING
IMPLEMENT_CLASS_HAS_INIT (Collider)
IMPLEMENT_OBJECT_SERIALIZE (Collider)
INSTANTIATE_TEMPLATE_TRANSFER (Collider)

#endif //ENABLE_PHYSICS
