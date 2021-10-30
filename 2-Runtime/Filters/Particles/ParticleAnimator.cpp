#include "UnityPrefix.h"
#include "ParticleAnimator.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Input/TimeManager.h"
#include "ParticleStruct.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/GameCode/DestroyDelayed.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

static Rand gParticleAnimRand (2);

void ParticleAnimator::GetColorAnimation(ColorRGBAf *col) const
{
	for(int i=0;i<kColorKeys;i++)
		col[i] = m_ColorAnimation[i]; 
}

void ParticleAnimator::SetColorAnimation(ColorRGBAf *col)
{
	for(int i=0;i<kColorKeys;i++)
		m_ColorAnimation[i] = col[i]; 
}

void ParticleAnimator::UpdateAnimator( ParticleArray& particles, PrivateParticleInfo& privateInfo, float deltaTime )
{
	// Quit updating if no new emissions and no old particles
	if( particles.empty() )
	{
		m_EnergylossFraction = 0.0F;
		
		if( WillAutoDestructIfNoParticles( privateInfo ) )
			DestroyObjectDelayed (GetGameObjectPtr());
		return;
	}
	if( m_Autodestruct == 1 )
		m_Autodestruct = 2;
	
	if( !m_StopSimulation )
	{
		privateInfo.aabb.Init ();
		UpdateParticles( particles, privateInfo, deltaTime );
		
		//Particle size is multipled to calculate the maximum size
		//Handle cases where grow size is negative also
		float endSize = privateInfo.maxEmitterParticleSize * pow (1.0F + m_SizeGrow, privateInfo.maxEnergy);
		privateInfo.maxParticleSize = m_SizeGrow < 0.0f ? privateInfo.maxEmitterParticleSize : endSize;
	}
}

bool ParticleAnimator::WillAutoDestructIfNoParticles( const PrivateParticleInfo& privateInfo ) const
{
	// Autodestruct destroys the GO if enabled AND either of those:
	// * Particles have been emitted, but are all dead now.
	// * Emit was once On, but now is Off.
	if( m_Autodestruct != 0 && IsWorldPlaying() )
	{
		if( m_Autodestruct == 2 || (privateInfo.hadEverEmitOn && !privateInfo.isEmitOn) )
			return true;
	}
	return false;
}


void ParticleAnimator::UpdateParticles (ParticleArray& particles, PrivateParticleInfo& privateInfo, float deltaTime) const
{
	Vector3f dtForce = m_Force * deltaTime;
	Vector3f rotationAxis;
	if (privateInfo.useWorldSpace)
		rotationAxis = (m_WorldRotationAxis + GetComponent(Transform).TransformDirection(m_LocalRotationAxis)) * deltaTime;
	else
		rotationAxis = (GetComponent(Transform).InverseTransformDirection(m_WorldRotationAxis) + m_LocalRotationAxis) * deltaTime;
					
	float damping = pow (m_Damping, deltaTime);
	Vector3f rndForce = m_RndForce * deltaTime;

	
	const float kColorScale = kColorKeys - 1.0f;

	// The color keys are inverted	
	ColorRGBA32 colorAnimation[kColorKeys] = { m_ColorAnimation[4], m_ColorAnimation[3], m_ColorAnimation[2], m_ColorAnimation[1], m_ColorAnimation[0] };
	float sizeScale = pow (1.0F + m_SizeGrow, deltaTime);
	float maxColorAnimationT = (float)kColorKeys - 1.0F - Vector3f::epsilon;
	
	bool doesAnimateColor = m_DoesAnimateColor;
	int particleSize = particles.size ();
	int i = 0;
	while (i < particleSize)
	{
		Particle& p = particles[i];

		// Update Alpha 
		p.energy -= deltaTime;

		if (p.energy <= 0.0f) {
			// Kill particle (replace particle i with last, and continue updating the moved particle)
			KillParticle (particles, i);
			--particleSize;
			continue;
		}

		if( doesAnimateColor )
		{
			// [0..kColorKeys-1] for [0..maxEnergy]
			float t = p.energy * kColorScale / p.startEnergy;
			t = FloatMin (t, maxColorAnimationT);
			int baseColor = FloorfToIntPos (t);
			DebugAssertIf( baseColor < 0 || baseColor >= kColorKeys - 1 );
			float frac = t - (float)baseColor;
			int intFrac = RoundfToIntPos (frac * 255.0F);
			p.color = Lerp (colorAnimation[baseColor], colorAnimation[baseColor + 1], intFrac);
		}
						
		// Update Velocity
		p.velocity *= damping;
		
		p.velocity += dtForce;

		p.velocity += RandomPointInsideCube (gParticleAnimRand, rndForce);

		p.velocity += Cross (rotationAxis, p.velocity);
		
		// Update position
		p.position += p.velocity * deltaTime;

		// Update Size
		p.size *= sizeScale;
		
		// Update Bounding Box
		privateInfo.aabb.Encapsulate (p.position);
		
		p.rotation += p.angularVelocity * deltaTime;

		Prefetch(&particles[i] + 8);
		Prefetch(&particles[particleSize-1]);
		
		i++;
	}
}

ParticleAnimator::ParticleAnimator(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_ColorAnimation[0] = ColorRGBA32 (255, 255, 255, 10);
	m_ColorAnimation[1] = ColorRGBA32 (255, 255, 255, 180);
	m_ColorAnimation[2] = ColorRGBA32 (255, 255, 255, 255);
	m_ColorAnimation[3] = ColorRGBA32 (255, 255, 255, 180);
	m_ColorAnimation[4] = ColorRGBA32 (255, 255, 255, 10);

	m_WorldRotationAxis = Vector3f::zero;
	m_LocalRotationAxis = Vector3f::zero;
	m_DoesAnimateColor  = true;
	m_RndForce          = Vector3f::zero;
	m_Force             = Vector3f::zero;	
	m_SizeGrow          = 0.0f;
	m_Damping	          = 1.0f;
	m_Autodestruct      = 0;
	m_StopSimulation    = false;
	m_EnergylossFraction = 0.0F;
}

ParticleAnimator::~ParticleAnimator ()
{
}

IMPLEMENT_CLASS_HAS_INIT (ParticleAnimator)
IMPLEMENT_OBJECT_SERIALIZE (ParticleAnimator)

static void ResetRandSeed ()
{
	gParticleAnimRand.SetSeed (2);
}

void ParticleAnimator::InitializeClass ()
{
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Register(ResetRandSeed);
}

void ParticleAnimator::CleanupClass ()
{
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Unregister(ResetRandSeed);
}

template<class TransferFunction> inline
void ParticleAnimator::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	
	transfer.Transfer (m_DoesAnimateColor, "Does Animate Color?");
	transfer.Align();
	transfer.Transfer( m_ColorAnimation[0], "colorAnimation[0]", kSimpleEditorMask );
	transfer.Transfer( m_ColorAnimation[1], "colorAnimation[1]", kSimpleEditorMask );
	transfer.Transfer( m_ColorAnimation[2], "colorAnimation[2]", kSimpleEditorMask );
	transfer.Transfer( m_ColorAnimation[3], "colorAnimation[3]", kSimpleEditorMask );
	transfer.Transfer( m_ColorAnimation[4], "colorAnimation[4]", kSimpleEditorMask );
	DebugAssertIf (kColorKeys != 5);
	
	transfer.Transfer( m_WorldRotationAxis, "worldRotationAxis" );
	transfer.Transfer( m_LocalRotationAxis, "localRotationAxis" );
	transfer.Transfer( m_SizeGrow, "sizeGrow", kSimpleEditorMask );
	transfer.Transfer( m_RndForce, "rndForce", kSimpleEditorMask );
	transfer.Transfer( m_Force, "force", kSimpleEditorMask );
	transfer.Transfer( m_Damping, "damping", kSimpleEditorMask ); m_Damping = clamp<float> (m_Damping, 0.0f, 1.0f);
	transfer.Transfer( m_StopSimulation, "stopSimulation", kHideInEditorMask );
	
	bool autodestruct = m_Autodestruct;
	transfer.Transfer (autodestruct, "autodestruct");
	if (transfer.IsReading ())
	{
		if (autodestruct == 0)
			m_Autodestruct = 0;
		else if (m_Autodestruct == 0)
			m_Autodestruct = 1;
	}
}
