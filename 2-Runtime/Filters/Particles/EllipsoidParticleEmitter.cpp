#include "UnityPrefix.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"
#include "EllipsoidParticleEmitter.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

using namespace std;

Rand gEllipsoidEmitterRand (3);

EllipsoidParticleEmitter::EllipsoidParticleEmitter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_Ellipsoid = Vector3f (1, 1, 1);
	m_MinEmitterRange = 0.0F;
}

EllipsoidParticleEmitter::~EllipsoidParticleEmitter ()
{
}

static void ResetEllipsoidEmitterRand ()
{
	gEllipsoidEmitterRand.SetSeed (3);
}

void EllipsoidParticleEmitter::InitializeClass ()
{
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Register(ResetEllipsoidEmitterRand);
}

void EllipsoidParticleEmitter::CleanupClass ()
{
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Unregister(ResetEllipsoidEmitterRand);
}


void EllipsoidParticleEmitter::SetupParticles (
	ParticleArray& particles,
	const Vector3f& velocityOffset,
	const Matrix3x3f& rotation,
	int firstIndex)
{
	float deltaTime = GetDeltaTime ();
	MinMaxAABB& aabb = m_PrivateInfo.aabb;
	for (int i = firstIndex;i<particles.size ();i++)
	{
		SetupParticle (particles[i], velocityOffset, rotation, deltaTime);
		aabb.Encapsulate (particles[i].position);
	}
}

void EllipsoidParticleEmitter::SetupParticle (
	Particle& p,
	const Vector3f& velocityOffset,
	const Matrix3x3f& rotation,
	float deltaTime)
{
	InitParticleEnergy(gEllipsoidEmitterRand, p, deltaTime);
	
	// Set particle starting position
	p.position  = m_PreviousEmitterPos;
	p.position += velocityOffset * RangedRandom (gEllipsoidEmitterRand, 0.0F, deltaTime);
	p.position += (m_EmitterPos - m_PreviousEmitterPos) * Random01 (gEllipsoidEmitterRand);
	Vector3f insideEllipsoidPosition = RandomPointBetweenEllipsoid (gEllipsoidEmitterRand, m_Ellipsoid, m_MinEmitterRange);
	p.position += rotation.MultiplyVector3 (insideEllipsoidPosition);
	
	// Set velocity
	p.velocity = velocityOffset;
	p.velocity += rotation.MultiplyVector3 (RandomPointInsideEllipsoid (gEllipsoidEmitterRand, m_RndVelocity));

	p.rotation = m_RndInitialRotations ? RangedRandom (gEllipsoidEmitterRand, 0.0f, 2*kPI):0.0F;
	float angularVelocity = m_AngularVelocity;
#if SUPPORT_REPRODUCE_LOG
	if (m_RndAngularVelocity > Vector3f::epsilon)
#endif
	angularVelocity += RangedRandom (gEllipsoidEmitterRand, -m_RndAngularVelocity, m_RndAngularVelocity);
	p.angularVelocity = Deg2Rad(angularVelocity);
	
//	Vector3f ellipsoidRelativeDirection = NormalizeSafe (insideEllipsoidPosition);
//	p.velocity += rotation.MultiplyVector3 (Cross (ellipsoidRelativeDirection, info.tangentVelocity));
	
	Vector3f uTangent (insideEllipsoidPosition.z, 0.0F, -insideEllipsoidPosition.x);
	Vector3f vTangent (insideEllipsoidPosition.x, 0.0F, -insideEllipsoidPosition.y);
	uTangent = NormalizeSafe (uTangent);
	vTangent = NormalizeSafe (vTangent);
	Vector3f normal = NormalizeSafe (insideEllipsoidPosition);
	p.velocity += rotation.MultiplyVector3 (uTangent * m_TangentVelocity.x);
	p.velocity += rotation.MultiplyVector3 (vTangent * m_TangentVelocity.y);
	p.velocity += rotation.MultiplyVector3 (normal * m_TangentVelocity.z);
	
	p.color = ColorRGBA32 (255, 255, 255, 255);
	
	// Set size
	p.size = RangedRandom (gEllipsoidEmitterRand, m_MinSize, m_MaxSize);
}

IMPLEMENT_CLASS_HAS_INIT (EllipsoidParticleEmitter)
IMPLEMENT_OBJECT_SERIALIZE (EllipsoidParticleEmitter)

template<class TransferFunction> inline
void EllipsoidParticleEmitter::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.Align();
	TRANSFER_SIMPLE (m_Ellipsoid);
	TRANSFER (m_MinEmitterRange);
}
