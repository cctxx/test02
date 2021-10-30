#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "WheelCollider.h"
#include "Runtime/Graphics/Transform.h"
#include "RigidBody.h"
#include "PhysicsManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/AABBUtility.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "Runtime/BaseClasses/IsPlaying.h"


#define GET_SHAPE() static_cast<NxWheelShape*>(m_Shape)


const float kMinSize = 0.00001F;
const float kMinMass = 0.00001F;

// we can't just reinterpret_cast one to another because NxTireFunctionDesc
// has a vtable (for apparently no good reason)
static inline void FrictionCurveToNovodexTireFunc( const WheelFrictionCurve& src, NxTireFunctionDesc& dst )
{
	dst.extremumSlip = src.extremumSlip;
	dst.extremumValue = src.extremumValue;
	dst.asymptoteSlip = src.asymptoteSlip;
	dst.asymptoteValue = src.asymptoteValue;
	dst.stiffnessFactor = src.stiffnessFactor;
}
static inline void NovodexTireFuncToFrictionCurve( const NxTireFunctionDesc& src, WheelFrictionCurve& dst )
{
	dst.extremumSlip = src.extremumSlip;
	dst.extremumValue = src.extremumValue;
	dst.asymptoteSlip = src.asymptoteSlip;
	dst.asymptoteValue = src.asymptoteValue;
	dst.stiffnessFactor = src.stiffnessFactor;
}


WheelCollider::WheelCollider(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	// set default parameters for tire curves
	NxTireFunctionDesc func;
	NovodexTireFuncToFrictionCurve( func, m_SidewaysFriction );
	NovodexTireFuncToFrictionCurve( func, m_ForwardFriction );
}

WheelCollider::~WheelCollider ()
{
}

void WheelCollider::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	if (m_Shape)
	{
		// Apply changed values
		SetCenter (m_Center);
		SetRadius (m_Radius);
		SetSuspensionDistance (m_SuspensionDistance);
		SetSuspensionSpring (m_SuspensionSpring);
		SetForwardFriction(m_ForwardFriction);
		SetSidewaysFriction(m_SidewaysFriction);
		SetMass(m_Mass);
	}
	
	Super::AwakeFromLoad (awakeMode);
}	

void WheelCollider::SmartReset()
{
	Super::SmartReset();
	AABB aabb;
	if (GetGameObjectPtr () && CalculateLocalAABB (GetGameObject (), &aabb))
	{
		SetCenter( aabb.GetCenter() );
		SetRadius( aabb.GetExtent ().y );
		SetSuspensionDistance( aabb.GetExtent ().y * 0.1f );
	}
	else
	{
		SetCenter( Vector3f::zero );
		SetRadius( 1.0F );
		SetSuspensionDistance( 0.1f );
	}
	
	// setup default friction curves
	WheelFrictionCurve curve;
	curve.extremumSlip = 1.0f;
	curve.extremumValue = 20000.0f;
	curve.asymptoteSlip = 2.0f;
	curve.asymptoteValue = 10000.0f;
	curve.stiffnessFactor = 1.0f;
	
	SetForwardFriction( curve );
	SetSidewaysFriction( curve );
}

void WheelCollider::Reset()
{
	Super::Reset();
	
	m_Center = Vector3f::zero;
	m_Radius = 1.0F;
	m_SuspensionDistance = 0.1F;
	
	m_SuspensionSpring.spring = 1.0f;
	m_SuspensionSpring.damper = 0.0f;
	m_SuspensionSpring.targetPosition = 0.0f;
	m_Mass =  1.0F;

	// setup default friction curves
	WheelFrictionCurve curve;
	curve.extremumSlip = 1.0f;
	curve.extremumValue = 20000.0f;
	curve.asymptoteSlip = 2.0f;
	curve.asymptoteValue = 10000.0f;
	curve.stiffnessFactor = 1.0f;
	
	m_ForwardFriction = curve;
	m_SidewaysFriction = curve;
}

Vector3f WheelCollider::GetGlobalCenter() const
{
	return GetComponent(Transform).TransformPoint( m_Center );
}

float WheelCollider::GetGlobalRadius() const
{
	Vector3f scale = GetComponent(Transform).GetWorldScaleLossy();
	return std::max( Abs(m_Radius*scale.y), kMinSize );
}

float WheelCollider::GetGlobalSuspensionDistance() const
{
	Vector3f scale = GetComponent(Transform).GetWorldScaleLossy();
	return std::max( Abs(m_SuspensionDistance*scale.y), kMinSize );
}

void WheelCollider::Create(const Rigidbody* ignoreAttachRigidbody)
{
	if( m_Shape )
		Cleanup();

	NxWheelShapeDesc shapeDesc;
	shapeDesc.radius = GetGlobalRadius();
	shapeDesc.suspensionTravel = GetGlobalSuspensionDistance();
	shapeDesc.suspension = (NxSpringDesc&)m_SuspensionSpring;
	shapeDesc.inverseWheelMass = 1.0F / std::max( m_Mass, kMinMass );
	shapeDesc.wheelFlags = NX_WF_INPUT_LNG_SLIPVELOCITY | NX_WF_INPUT_LAT_SLIPVELOCITY;
	shapeDesc.materialIndex = 2; // use default wheel material setup in CreateScene
	FrictionCurveToNovodexTireFunc( m_ForwardFriction, shapeDesc.longitudalTireForceFunction );
	FrictionCurveToNovodexTireFunc( m_SidewaysFriction, shapeDesc.lateralTireForceFunction );

	m_IsTrigger = false; // TBD: make something nicer!

	Rigidbody* body = FindNewAttachedRigidbody (ignoreAttachRigidbody);
	// WheelCollider requires a Rigidbody, or PhysX will give an error.
	if (body != NULL)
		FinalizeCreate( shapeDesc, false, ignoreAttachRigidbody );
	// Don't show error in Editor, since during setup people may create WheelColliders on empty GameObjects,
	// and later move to a Rigidbody. We don't want to spam the console with errors in that case.
	else 
#if UNITY_EDITOR
		if (IsWorldPlaying())
#endif
		ErrorStringObject("WheelCollider requires an attached Rigidbody to function.", this);
}

void WheelCollider::Cleanup()
{
	Super::Cleanup();
}

void WheelCollider::SetCenter( const Vector3f& center )
{
	if (m_Center != center)
	{
		m_Center = center;
		SetDirty();
	}
	
	if( GET_SHAPE () )
	{
		TransformChanged( Transform::kRotationChanged | Transform::kPositionChanged );
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
		RefreshPhysicsInEditMode();
	}
}

void WheelCollider::SetRadius( float f )
{
	if (m_Radius != f)
	{
		SetDirty();
		m_Radius = f;
	}
	
	PROFILE_MODIFY_STATIC_COLLIDER
	
	if( GET_SHAPE () )
	{
		GET_SHAPE()->setRadius( GetGlobalRadius() );
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
		RefreshPhysicsInEditMode();
	}
}

void WheelCollider::SetSuspensionDistance( float f )
{
	if (m_SuspensionDistance != f)
	{
		SetDirty();
		m_SuspensionDistance = f;
	}
	
	if( GET_SHAPE () )
	{
		GET_SHAPE()->setSuspensionTravel( GetGlobalSuspensionDistance() );
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
	}
}

void WheelCollider::SetSuspensionSpring( const JointSpring& spring )
{
	if (m_SuspensionSpring != spring)
	{
		SetDirty();
		m_SuspensionSpring = spring;
	}
	
	if( GET_SHAPE () )
	{
		GET_SHAPE()->setSuspension( (NxSpringDesc&)spring );
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
	}
}

void WheelCollider::SetSidewaysFriction( const WheelFrictionCurve& curve )
{
	if (m_SidewaysFriction != curve)
	{
		SetDirty();
		m_SidewaysFriction = curve;
	}
	
	if( GET_SHAPE () )
	{
		NxTireFunctionDesc func;
		FrictionCurveToNovodexTireFunc( curve, func );
		GET_SHAPE()->setLateralTireForceFunction( func );
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
	}
}

void WheelCollider::SetForwardFriction( const WheelFrictionCurve& curve )
{
	if (m_ForwardFriction != curve)
	{
		SetDirty();
		m_ForwardFriction = curve;
	}
	
	if( GET_SHAPE () )
	{
		NxTireFunctionDesc func;
		FrictionCurveToNovodexTireFunc( curve, func );
		GET_SHAPE()->setLongitudalTireForceFunction( func );
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
	}
}

void WheelCollider::SetMass( float f )
{
	if (m_Mass != f)
	{
		SetDirty();
		m_Mass = std::max( f, kMinMass );
	}
	
	if( GET_SHAPE() )
	{
		GET_SHAPE()->setInverseWheelMass( 1.0F / m_Mass );
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
	}
}

void WheelCollider::SetMotorTorque( float f )
{
	if( m_Shape )
	{
		GET_SHAPE()->setMotorTorque( f );
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
	}
}

float WheelCollider::GetMotorTorque() const
{
	if( m_Shape )
		return GET_SHAPE()->getMotorTorque();
	else
		return 0.0F;
}

void WheelCollider::SetBrakeTorque( float f )
{
	if( m_Shape )
	{
		GET_SHAPE()->setBrakeTorque( f );
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
	}
}

float WheelCollider::GetBrakeTorque() const
{
	if( m_Shape )
		return GET_SHAPE()->getBrakeTorque();
	else
		return 0.0F;
}

void WheelCollider::SetSteerAngle( float f )
{
	if( m_Shape )
	{
		GET_SHAPE()->setSteerAngle( Deg2Rad(f) );
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
	}
}

float WheelCollider::GetSteerAngle() const
{
	if( m_Shape )
		return Rad2Deg( GET_SHAPE()->getSteerAngle() );
	else
		return 0.0F;
}

bool WheelCollider::IsGrounded() const
{
	if( m_Shape )
	{
		NxWheelContactData contactData;
		NxShape* otherShape = GET_SHAPE()->getContact( contactData );
		return ( otherShape != NULL );
	}
	else
	{
		return false;
	}
}

bool WheelCollider::GetGroundHit( WheelHit& hit ) const
{
	if( m_Shape )
	{
		NxWheelContactData contactData;
		NxShape* otherShape = GET_SHAPE()->getContact( contactData );
		if( otherShape != NULL )
		{
			hit.point = reinterpret_cast<Vector3f&>( contactData.contactPoint );
			hit.normal = reinterpret_cast<Vector3f&>( contactData.contactNormal );
			hit.forwardDir = reinterpret_cast<Vector3f&>( contactData.longitudalDirection );
			hit.sidewaysDir = reinterpret_cast<Vector3f&>( contactData.lateralDirection );
			hit.force = contactData.contactForce;
			hit.forwardSlip = contactData.longitudalSlip;
			hit.sidewaysSlip = contactData.lateralSlip;
			hit.collider = reinterpret_cast<Collider*>( otherShape->userData );
			return true;
		}
		return false;
	}
	else
	{
		return false;
	}
}

float WheelCollider::GetRpm() const
{
	if( m_Shape )
		return GET_SHAPE()->getAxleSpeed() / (kPI*2.0) * 60.0;
	else
		return 0.0F;
}


void WheelCollider::ScaleChanged()
{
	NxWheelShape* shape = GET_SHAPE();
	shape->setRadius( GetGlobalRadius() );
	shape->setSuspensionTravel( GetGlobalSuspensionDistance() );
	m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
	RefreshPhysicsInEditMode();
}

void WheelCollider::FetchPoseFromTransform()
{
	FetchPoseFromTransformUtility( m_Center );
}

bool WheelCollider::GetRelativeToParentPositionAndRotation( Transform& transform, Transform& anyParent, Matrix4x4f& matrix )
{
	return GetRelativeToParentPositionAndRotationUtility( transform, anyParent, m_Center, matrix );
}

void WheelCollider::TransformChanged( int changeMask )
{
	if( m_Shape )
	{
		if( m_Shape->getActor().userData == NULL )
		{	
			PROFILER_AUTO(gStaticColliderMove, this)
			FetchPoseFromTransform();
		}
		else
		{
			Rigidbody* body = (Rigidbody*)m_Shape->getActor().userData;
			Matrix4x4f matrix;
			if (GetRelativeToParentPositionAndRotation( GetComponent(Transform), body->GetComponent(Transform), matrix ))
			{
				NxMat34 shapeMatrix;
				shapeMatrix.setColumnMajor44( matrix.GetPtr () );
				m_Shape->setLocalPose( shapeMatrix );
			}
		}

		if( changeMask & Transform::kScaleChanged )
			ScaleChanged();
		
		m_Shape->getActor().wakeUp(); // wake actor up in case it was sleeping
		RefreshPhysicsInEditMode();
	}
}

void WheelCollider::InitializeClass ()
{
	REGISTER_MESSAGE( WheelCollider, kTransformChanged, TransformChanged, int );
}

template<class TransferFunction>
void WheelCollider::Transfer (TransferFunction& transfer) 
{
	// NOTE: bypass Collider transfer, so that material and trigger flag are not
	// serialized.
	
	Super::Super::Transfer( transfer );
	TRANSFER( m_Center );
	TRANSFER_SIMPLE( m_Radius );
	TRANSFER_SIMPLE( m_SuspensionDistance );
	TRANSFER_SIMPLE( m_SuspensionSpring );
	TRANSFER_SIMPLE( m_Mass );
	 
	TRANSFER( m_ForwardFriction );
	TRANSFER( m_SidewaysFriction );

	// Since we bypass Collider transfer, we need to transfer m_Enabled ourselves
	transfer.Transfer (m_Enabled, "m_Enabled", kHideInEditorMask | kEditorDisplaysCheckBoxMask);
	transfer.Align();
}

IMPLEMENT_CLASS_HAS_INIT( WheelCollider )
IMPLEMENT_OBJECT_SERIALIZE( WheelCollider )
#endif //ENABLE_PHYSICS