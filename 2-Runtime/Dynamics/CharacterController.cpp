#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#if ENABLE_PHYSICS
#include "../Dynamics/CharacterController.h"
#include "External/PhysX/builds/SDKs/NxCharacter/include/NxController.h"
#include "External/PhysX/builds/SDKs/NxCharacter/include/Controller.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "External/PhysX/builds/SDKs/NxCharacter/include/NxCapsuleController.h"
#include "External/PhysX/builds/SDKs/NxCharacter/include/ControllerManager.h"
#include "PhysicsManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Collider.h"
#include "Runtime/Filters/AABBUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "NxWrapperUtility.h"
#include "Runtime/BaseClasses/MessageHandler.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/GameCode/RootMotionData.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Scripting/Scripting.h"

using namespace std;

#include "Runtime/Mono/MonoBehaviour.h"

///@TODO: Take advantage of sweep caching by storing when static objects colliders 
///               move and only in that case reset the  sweep cache.

///@TODO: DO A PROPER SendMessage for the character controller

inline CharacterController* GetUnityController(NxController* controller)
{
	return (CharacterController*)((Controller*)controller->getUserData());
}

struct ControllerHitReport : NxUserControllerHitReport
{
	struct RecordedControllerColliderHit
	{
		Collider*			collider;
		Vector3f			point;
		Vector3f			normal;
		Vector3f			motionDirection;
		float				motionLength;
	};

	typedef std::vector<RecordedControllerColliderHit> RecordedControllerColliderHits;
	RecordedControllerColliderHits m_Record;

	virtual NxControllerAction onShapeHit(const NxControllerShapeHit& hit)
	{
		CharacterController* controller = GetUnityController(hit.controller);
		GameObject* go = controller->GetGameObjectPtr ();
		
		// Do not allocate the mono message if the game object is not interested, to keep GC down.
		// See Case #366569.
		if (go && go->WillHandleMessage(kControllerColliderHit))
		{
			m_Record.push_back(RecordedControllerColliderHit());
			RecordedControllerColliderHit& controllerHit = m_Record.back();
									
			controllerHit.point = NxExtendedToVec3(hit.worldPos);
			controllerHit.normal = (const Vector3f&)hit.worldNormal;
			controllerHit.motionDirection = (const Vector3f&)hit.dir;
			controllerHit.motionLength = hit.length;
			controllerHit.collider = (Collider*)hit.shape->userData;
		}
		return NX_ACTION_NONE;
	}
	
	virtual NxControllerAction onControllerHit(const NxControllersHit& hit)
	{
		return NX_ACTION_NONE;
	}
};


#if ENABLE_PHYSICS
static ControllerHitReport gControllerHitReport;
// This needs to be a pointer, so we can control initialization and destruction order.
static ControllerManager *gControllerManager = NULL;
#endif

CharacterController::CharacterController(MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{
	m_Controller = NULL;
	m_VerticalSpeed = 0.0F;
	m_LastCollisionFlags = 0;
	m_Velocity = Vector3f(0,0,0);
	m_LastSimpleVelocity = Vector3f(0,0,0);
	m_DetectCollision = true;
}

CharacterController::~CharacterController ()
{
}

void CharacterController::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	if(IsActive())
		Create(NULL);
	
	Super::AwakeFromLoad (awakeMode);
}	

void CharacterController::SmartReset()
{
	Super::SmartReset();
	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		Vector3f extents = aabb.GetCenter () + aabb.GetExtent ();
		SetRadius (max (extents.x, extents.z));
		SetHeight (extents.y * 2.0F);
	}
	else
	{
		SetRadius (0.5F);
		SetHeight (2.0F);
	}
}


void CharacterController::Reset()
{
	Super::Reset();

	m_Center = Vector3f::zero;
	m_Radius = 0.5F;
	m_Height = 2.0F;
	
	m_MinMoveDistance = .00F;
	m_SkinWidth = max(GetRadius() * 0.16F, 0.01F);
	m_StepOffset = 0.3f;
	m_Height = 2.0F;
	m_Radius = 0.3f;
	m_SlopeLimit = 45.0F;
}

int CharacterController::Move (const Vector3f& movement)
{
	if( !m_Controller )
	{
		ScriptWarning("CharacterController.Move called on inactive controller", GetGameObjectPtr());
		return 0;
	}
	unsigned int touching = 0;
	Vector3f oldPos = NxExtendedToVec3(m_Controller->getDebugPosition());

	m_Controller->reportSceneChanged();
	
	Assert(gControllerHitReport.m_Record.empty());
	
	m_Controller->move((const NxVec3&)movement, GetPhysicsManager().GetLayerCollisionMask(GetGameObject ().GetLayer ()), m_MinMoveDistance, touching, 1.0F);

	bool oldDisableDestruction = GetDisableImmediateDestruction();
	SetDisableImmediateDestruction (true);

	// Swap hits to get consistent callbacks
	// CharacterController might call CharacterController.Move from inside a OnControllerColliderHit callback.
	ControllerHitReport::RecordedControllerColliderHits hits;
	hits.swap(gControllerHitReport.m_Record);
	Assert(gControllerHitReport.m_Record.empty());
	
	for (ControllerHitReport::RecordedControllerColliderHits::iterator i = hits.begin(); i != hits.end(); i++)
	{
		#if ENABLE_SCRIPTING
		const ControllerHitReport::RecordedControllerColliderHit& recordedHit = *i;
		
		ControllerColliderHit controllerHit;		

		controllerHit.point = recordedHit.point;
		controllerHit.normal = recordedHit.normal;
		controllerHit.motionDirection = recordedHit.motionDirection;
		controllerHit.motionLength = recordedHit.motionLength;
		controllerHit.controller = Scripting::ScriptingWrapperFor(this);
		controllerHit.collider = Scripting::ScriptingWrapperFor(recordedHit.collider);
		controllerHit.push = false;

		ScriptingObjectPtr mono = CreateScriptingObjectFromNativeStruct(GetScriptingManager ().GetCommonClasses ().controllerColliderHit, controllerHit);

		MessageData data;
		data.SetScriptingObjectData(mono);

		SendMessageAny (kControllerColliderHit, data);
		#endif
		
		if (!m_Controller)
		{
			SetDisableImmediateDestruction (oldDisableDestruction);
			return touching;
		}
	}
	
	SetDisableImmediateDestruction (oldDisableDestruction);

	m_LastCollisionFlags = touching;

	// Stop gravity when touching ground
	if ((touching & NXCC_COLLISION_DOWN) && m_VerticalSpeed < 0.0F)
		m_VerticalSpeed = 0.0F;

	// At least sync the actual position. Update controller will then reset the position to the filtered position!
	Transform& transform = GetComponent(Transform);
	Vector3f pos = NxExtendedToVec3(m_Controller->getFilteredPosition());
	
	m_Velocity = (pos - oldPos) * GetInvDeltaTime();
	
	GameObject::GetMessageHandler ().SetMessageEnabled (ClassID(CharacterController), kTransformChanged.messageID, false);
	transform.SetPositionWithLocalOffset (pos, m_Center);
	GameObject::GetMessageHandler ().SetMessageEnabled (ClassID(CharacterController), kTransformChanged.messageID, true);

	return touching;
}

bool CharacterController::IsGrounded ()
{
	return m_LastCollisionFlags & NXCC_COLLISION_DOWN;
}

Vector3f CharacterController::GetVelocity()
{
	return m_Velocity;
}

void CharacterController::Create (const Rigidbody* ignoreAttachRigidbody)
{
	NxCapsuleControllerDesc desc;
	desc.slopeLimit = Cos (Deg2Rad (m_SlopeLimit));

	if(desc.slopeLimit<0.0f)
		desc.slopeLimit=0.0f;

	desc.skinWidth = m_SkinWidth;
	desc.stepOffset = m_StepOffset;
	desc.userData = this;
	Vector2f extents = GetGlobalExtents ();
	desc.radius = extents.x;
	desc.height = extents.y;
	desc.interactionFlag = NXIF_INTERACTION_USE_FILTER;
	
	desc.callback = &gControllerHitReport;
	desc.position = Vec3ToNxExtended(GetWorldCenterPosition());
	if (m_Controller)
	{
		gControllerManager->releaseController(*m_Controller);
	}
	
	m_Controller = (NxCapsuleController*)gControllerManager->createController(&GetDynamicsScene(), desc);
	m_Shape = m_Controller->getActor()->getShapes()[0];
	m_Shape->userData = this;
	m_Shape->setGroup (GetGameObject ().GetLayer ());	
	m_Controller->setCollision(m_DetectCollision);

//	printf_console ("Create shape %d userData: %d (%s, %s)\n", m_Shape, m_Shape->userData, GetName().c_str(), GetClassName().c_str());
	
}

Vector3f CharacterController::GetWorldCenterPosition() const
{
	const Transform& transform = GetComponent(Transform);
	return transform.TransformPoint (m_Center);
	
}

AABB CharacterController::GetBounds ()
{
	if (m_Shape)
	{
		// AABB reported by PhysX is inaccurate, as PhysX will just transform the local AABB.
		// For Spheres it's very easy to do better. Also needed for Editor selection bounds,
		// as PhysX only updates Character Controller bounds in play mode.
		Transform& transform = GetComponent (Transform);
		Vector3f p = transform.TransformPoint (m_Center);
				
		Vector2f extents = GetGlobalExtents();
		
		Vector3f center1 = p + Vector3f(0, extents.y * 0.5, 0);
		Vector3f center2 = p + Vector3f(0, -extents.y * 0.5, 0);
		
		// Make AABB of both global centers
		AABB aabb (center1, Vector3f::zero);
		aabb.Encapsulate (center2);
		
		// Expand by global radius
		aabb.m_Extent += Vector3f(extents.x, extents.x, extents.x);
		return aabb;
	}
	else 
		return Super::GetBounds ();
}

void CharacterController::Cleanup ()
{
	if (m_Controller)
	{
		gControllerManager->releaseController(*m_Controller);
		m_Controller = NULL;
		m_Shape = NULL;
	}
}

Vector2f CharacterController::GetGlobalExtents () const
{
	const float kMinSize = 0.00001F;

	Vector3f scale = GetComponent (Transform).GetWorldScaleLossy ();

	float absoluteHeight = max (Abs (m_Height * scale.y), kMinSize);
	float absoluteRadius = max (Abs (scale.x), Abs (scale.z)) * m_Radius;
	
	float height = absoluteHeight - absoluteRadius * 2.0F;
	
	height = max (height, kMinSize);
	absoluteRadius = max (absoluteRadius, kMinSize);

	return Vector2f (absoluteRadius, height);
}

void CharacterController::TransformChanged(int change)
{
	// Do not call Super::TransformChanged (change) here. The Controller base class
	// uses transformChanged to check for reparenting to a new rigidbody. In the CharacterController
	// case there is no rigibody attachment, which causes it to always reparent, breaking
	// trigger interactions.
	
	if (m_Controller == NULL)
		return;
		
	if (change & Transform::kScaleChanged)
	{
		Vector2f extents = GetGlobalExtents ();
		m_Controller->setRadius(extents.x);
		m_Controller->setHeight(extents.y);
	}

	// Teleport
	if (change & Transform::kPositionChanged)
	{
		m_Controller->setPosition (Vec3ToNxExtended(GetWorldCenterPosition()));
		m_VerticalSpeed = 0.0F;
	}
}

template<class TransferFunction>
void CharacterController::Transfer(TransferFunction& transfer)
{
	Super::Super::Transfer (transfer);

	transfer.SetVersion (2);

	TRANSFER_SIMPLE (m_Height);
	TRANSFER_SIMPLE (m_Radius);
	TRANSFER_SIMPLE (m_SlopeLimit);
	TRANSFER_SIMPLE (m_StepOffset);
	TRANSFER_SIMPLE (m_SkinWidth);

	TRANSFER (m_MinMoveDistance);
	TRANSFER (m_Center);

	if (transfer.IsVersionSmallerOrEqual (1))
	{
		// The PhysX character controller has been fixed so it works properly.
		// Before the fix, the character controller was unable to climb any
		// wall above 45 degress, regardless of the slope limit setting.
		// The slope limit is therefore clamped to 45 degrees when loading old
		// projects to mimic the old behaviour.
		m_SlopeLimit = std::min (45.0F, m_SlopeLimit);
	}
}

void CharacterController::ScaleChanged()
{
	if (m_Controller)
	{
		Vector2f extents = GetGlobalExtents();
		m_Controller->setRadius(extents.x);
		m_Controller->setHeight(extents.y);
	}
}

void CharacterController::SetRadius(float radius)
{
	m_Radius = radius;
	SetDirty();
	if (m_Controller)
	{
		Vector2f extents = GetGlobalExtents();
		m_Controller->setRadius(extents.x);
		m_Controller->setHeight(extents.y);
	}
}

void CharacterController::SetHeight(float height)
{
	m_Height = height;
	SetDirty();
	if (m_Controller)
	{
		Vector2f extents = GetGlobalExtents();
		m_Controller->setRadius(extents.x);
		m_Controller->setHeight(extents.y);
	}
}

bool CharacterController::SimpleMove (const Vector3f& speed)
{
	float dt = GetDeltaTime();

	m_VerticalSpeed += GetPhysicsManager().GetGravity().y * dt;
	Vector3f offset;
	
	if (IsGrounded())
	{
		offset = Vector3f(speed.x, m_VerticalSpeed, speed.z);
		m_LastSimpleVelocity = offset;
	}
	else
		offset = Vector3f(m_LastSimpleVelocity.x, m_VerticalSpeed, m_LastSimpleVelocity.z);
	
	offset *= dt;
	Move(offset);
	
	return IsGrounded();
}

void CharacterController::ApplyRootMotionBuiltin (RootMotionData* rootMotion)
{
	if(!GetEnabled())
		return;

	float deltaTime = GetDeltaTime();
	
	// Get the Y velocity according to rigidbody or own Y speed for CharacterController
	m_VerticalSpeed += GetPhysicsManager().GetGravity().y * deltaTime;

	// Get the velocity from root motion.
	// Blend physics velocity with animation velocity on y-axis
	Vector3f deltaMotion;
	deltaMotion   = rootMotion->deltaPosition;
	deltaMotion.y = Lerp (deltaMotion.y, m_VerticalSpeed * deltaTime, clamp01(rootMotion->gravityWeight));
	
	// Apply velocity and rotation
	Move (deltaMotion);

	GetComponent(Transform).SetRotation(rootMotion->targetRotation);
	
	rootMotion->didApply = true;
}

void CharacterController::InitializeClass ()
{
	REGISTER_MESSAGE_PTR (CharacterController, kAnimatorMoveBuiltin, ApplyRootMotionBuiltin, RootMotionData);
}

void CharacterController::CleanupClass ()
{
}


/*
Jumpheight equation:
v = -(currentVelocity  + sqrt(2 * h * g));
*/
/*
bool CharacterController::SimpleJump (const Vector3f& movement, float jumpHeight)
{
	float dt = GetDeltaTime();

	bool didJump = IsGrounded();
	if (didJump)
	{
//		height = 0.5 * Sqr(GetPhysicsManager().GetGravity().y) + velocity;
		m_LastCollisionFlags &= ~NXCC_COLLISION_DOWN;
		
//		0 = g * t + velocity
		
//		velocity * time + 0.5 * GetPhysicsManager().GetGravity().y * t ^ 2
	}
	
	SimpleMove(movement);
	
	if (didJump)
		m_LastCollisionFlags &= ~NXCC_COLLISION_DOWN;
	
	return didJump;
}
*/

float CharacterController::GetSlopeLimit ()
{
	return m_SlopeLimit;
}

void CharacterController::SetSlopeLimit (float limit)
{
	m_SlopeLimit = limit;
	SetDirty();
	if (m_Controller)
		Create (NULL);
}

float CharacterController::GetStepOffset ()
{
	return m_StepOffset;
}

void CharacterController::SetStepOffset (float limit)
{
	m_StepOffset = limit;
	SetDirty();
	if (m_Controller)
		m_Controller->setStepOffset(limit);
}

Vector3f CharacterController::GetCenter ()
{
	return m_Center;
}

void CharacterController::SetCenter(const Vector3f& center)
{
	m_Center = center;
	if (m_Controller)
		Create(NULL);
	SetDirty();
}

void CharacterController::SetDetectCollisions (bool detect)
{
	m_DetectCollision = detect;
	if (m_Controller)
		m_Controller->setCollision(m_DetectCollision);
}

void CharacterController::SetIsTrigger (bool trigger)
{
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1))
	{
		if (trigger)
			ErrorStringObject ("A Character Controller cannot be a trigger.", this);
		m_IsTrigger = false;
	}
	else
		Super::SetIsTrigger (trigger);
}


void CharacterController::CreateControllerManager ()
{
	Assert (gControllerManager == NULL);
	gControllerManager = new ControllerManager();
}

void CharacterController::CleanupControllerManager ()
{
	Assert (gControllerManager != NULL);
	delete gControllerManager;
}

IMPLEMENT_CLASS_HAS_INIT(CharacterController)
IMPLEMENT_OBJECT_SERIALIZE(CharacterController)

#endif
