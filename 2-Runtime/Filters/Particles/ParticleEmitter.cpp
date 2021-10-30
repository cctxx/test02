#include "UnityPrefix.h"
#include "ParticleEmitter.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "ParticleAnimator.h"
#include "WorldParticleCollider.h"
#include "ParticleRenderer.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"


using namespace std;

static Rand gEmitterRand (5);

enum { kMaxParticleCount =  65000 / 4 };
const float kMaxEnergy = 1e20f;

typedef List< ListNode<ParticleEmitter> > ParticleEmitterList;
static ParticleEmitterList gActiveEmitters;


ParticleEmitter::ParticleEmitter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_EmittersListNode( this )
{
	m_EmissionFrac = 0.0F;
	m_Emit = true;
	m_Enabled = true;
	m_OneShot = false;
	m_PrivateInfo.hadEverEmitOn = false;
	m_PrivateInfo.isEmitOn = false;

	m_MinSize = 0.1F;
	m_MaxSize = 0.1F;
	m_MinEnergy = 3.0F;
	m_MaxEnergy = 3.0F;
	m_MinEmission = 50.0F;
	m_MaxEmission = 50.0F;
	m_UseWorldSpace = true;
	m_EmitterVelocityScale = 0.05F;
	m_WorldVelocity = Vector3f::zero;
	m_LocalVelocity = Vector3f::zero;
	m_TangentVelocity = Vector3f::zero;
	m_RndVelocity = Vector3f::zero;	
	m_RndAngularVelocity = 0.0F;
	m_AngularVelocity = 0.0F;
	m_RndInitialRotations = false;
	m_FirstFrame = true;
	m_PrivateInfo.aabb.Init ();	
}

ParticleEmitter::~ParticleEmitter ()
{
}

void ParticleEmitter::UpdateManagerState( bool updateParticles )
{
	if( updateParticles == m_EmittersListNode.IsInList() )
		return;
	if( updateParticles )
		gActiveEmitters.push_back(m_EmittersListNode);
	else
		m_EmittersListNode.RemoveFromList();
}

void ParticleEmitter::UpdateAllParticleSystems()
{
	const float deltaTimeEpsilon = 0.0001f;
	float deltaTime = GetDeltaTime();
	if(deltaTime < deltaTimeEpsilon)
		return;

	ParticleEmitterList::iterator next;
	for( ParticleEmitterList::iterator i = gActiveEmitters.begin(); i != gActiveEmitters.end(); i = next )
	{
		next = i;
		next++;
		ParticleEmitter& emitter = **i;
		emitter.UpdateParticleSystem(deltaTime);
	}
}



void ParticleEmitter::ResetEmitterPos ()
{
	if (m_UseWorldSpace)
		m_EmitterPos = GetComponent (Transform).GetPosition ();
	else
		m_EmitterPos = Vector3f::zero;
	m_PreviousEmitterPos = m_EmitterPos;
}


void ParticleEmitter::Deactivate( DeactivateOperation operation )
{
	Super::Deactivate(operation);
	
	m_PrivateInfo.hadEverEmitOn = false;
	UpdateManagerState( false );
}

PROFILER_INFORMATION(gParticleEmitterProfile, "Particle.Update", kProfilerParticles)

void ParticleEmitter::UpdateParticleSystem(float deltaTime)
{
	PROFILER_AUTO(gParticleEmitterProfile, this)
	
	if( !IsActive() )
	{
		AssertStringObject( "UpdateParticle system should not happen on disabled GO", this );
		return;
	}
	
	//@TODO: REMOVE FROM LIST PROPERLY TO SAVE ITERATING THROUGH ALL PARTICLES
	if (m_Enabled)
	{
		m_PrivateInfo.maxParticleSize = m_MaxSize;
		m_PrivateInfo.maxEmitterParticleSize = m_MaxSize;
        m_PrivateInfo.maxEnergy = clamp( m_MaxEnergy, 0.0f, kMaxEnergy );
		m_PrivateInfo.useWorldSpace = m_UseWorldSpace;
		if( m_Emit )
			m_PrivateInfo.hadEverEmitOn = true;
		m_PrivateInfo.isEmitOn = m_Emit;
	
		// 
		// Emit particles.

		ParticleAnimator* animator = QueryComponent(ParticleAnimator);
		if( IsEmitting() )
		{
			// We don't want to emit anymore if we have OneShot and auto destruct
			// in particle animator is true. Otherwise we'd emit when we have no particles, and
			// there will never be zero particles, hence no auto destruct ever!
			bool willAutoDestruct = false;
			if( animator && m_OneShot && m_Particles.empty() )
				willAutoDestruct = animator->WillAutoDestructIfNoParticles( m_PrivateInfo );
			if( !willAutoDestruct )
				TimedEmit( deltaTime );
		}

		//
		// Update particle animator

		if( animator )
			animator->UpdateAnimator( m_Particles, m_PrivateInfo, deltaTime );
			
		//
		// Update world particle collider

		WorldParticleCollider* collider = QueryComponent(WorldParticleCollider);
		if( collider )
			collider->UpdateParticleCollider( m_Particles, m_PrivateInfo, deltaTime );
	
		//
		// Update renderer
	
		ParticleRenderer* renderer = QueryComponent(ParticleRenderer);
		if( renderer )
			renderer->UpdateParticleRenderer();
	}
}

void ParticleEmitter::SetEmit (bool emit)
{
	if (m_Emit == emit)
		return;
	m_Emit = emit;
	UpdateManagerState( IsActive() );
	if (emit) {
		ResetEmitterPos ();
	}
}

void ParticleEmitter::SetEnabled (bool enabled)
{
	if (m_Enabled != enabled)
	{
		m_Enabled = enabled;
		SetDirty();
	}
}

void ParticleEmitter::Emit (unsigned int newParticleCount, float invDeltaTime) 
{
	if (newParticleCount <= 0)
		return;
	
	if (m_FirstFrame)
	{
		ResetEmitterPos();
		m_FirstFrame = false;
	}
	
	unsigned int firstNewIndex = m_Particles.size ();
	newParticleCount = min<unsigned int> (newParticleCount + firstNewIndex, kMaxParticleCount);
	
	if (newParticleCount != firstNewIndex)
	{
		m_Particles.resize (newParticleCount);
		
		Vector3f velocityOffset;
		Matrix3x3f localRotation;
			
		CalcOffsets (&velocityOffset, &localRotation, invDeltaTime);
		// Setup particles
		SetupParticles (m_Particles, velocityOffset, localRotation, firstNewIndex);
	}
}


void ParticleEmitter::Emit (const Vector3f &pos, const Vector3f &dir, float size, float energy, const ColorRGBA32 &color, float rotation, float angularVelocity) {
	
	if (m_Particles.size() >= kMaxParticleCount)
		return;
	
	Particle p;
	p.position = pos;
	p.velocity = dir;
	p.size = size;
	p.rotation = Deg2Rad(rotation);
	p.angularVelocity = Deg2Rad(angularVelocity);
//	p.energy = SecondsToEnergy (energy + GetDeltaTime ()) + 1;
	p.energy = energy;
	p.startEnergy = energy;
	p.color = color;
	m_Particles.push_back (p);
	m_PrivateInfo.aabb.Encapsulate( pos );
	UpdateManagerState( IsActive() );
}
	

void ParticleEmitter::ClearParticles () 
{
	m_Particles.clear();
	UpdateManagerState( IsActive() );
}

void ParticleEmitter::TimedEmit (float deltaTime)
{
	// Reserve enough memory to hold all particles that could ever be emitted with the current settings
	int maxParticles = 0;
	if (!m_OneShot)
	{
		maxParticles = CeilfToIntPos( min<float>(m_MaxEmission * m_MaxEnergy, kMaxParticleCount) );
	}
	else
	{
		maxParticles = RoundfToIntPos( min<float>(m_MaxEmission, kMaxParticleCount) );
	}	

	m_Particles.reserve (maxParticles);

	// Calculate how many new particles to emit
	// never emit more particles than the capacity.
	// the capacity can sometimes be too small if deltaTime is very large
	// or minEnergy == maxEnergy or minEmissions == maxEmissions
	int newParticleCount = 0;
	
	float emission = min<float> ( RangedRandom (gEmitterRand, m_MinEmission, m_MaxEmission), maxParticles );
	if (!m_OneShot)
	{
		float newParticleCountf = emission * deltaTime + m_EmissionFrac;
		newParticleCount = FloorfToIntPos (newParticleCountf);
		m_EmissionFrac = newParticleCountf - (float)newParticleCount;
	}
	else if(m_Particles.size () == 0) 
	{
		newParticleCount = RoundfToIntPos(emission);
	}

	newParticleCount = min<int> (m_Particles.capacity () - m_Particles.size (), newParticleCount);
	
	// Calculate velocity and rotation that is used to setup the particles
	// We do this here so we don't get confused by calls to Emit from outside the filter loop.
	if (m_UseWorldSpace)
	{
		m_PreviousEmitterPos = m_EmitterPos;
		m_EmitterPos = GetComponent (Transform).GetPosition ();
	}
	else
	{
		m_PreviousEmitterPos = Vector3f::zero;
		m_EmitterPos = Vector3f::zero;		
	}

	if (newParticleCount > 0)
	{
		Emit (newParticleCount, CalcInvDeltaTime(deltaTime));
	}
}

void ParticleEmitter::CalcOffsets (Vector3f *velocityOffset, Matrix3x3f *localRotation, float invDeltaTime) {
	Transform& t = GetComponent (Transform);
	if (m_UseWorldSpace)
	{
		m_EmitterPos = t.GetPosition ();

		QuaternionToMatrix (t.GetRotation (), *localRotation);

		*velocityOffset  = localRotation->MultiplyVector3 (m_LocalVelocity);
		*velocityOffset += m_WorldVelocity;
		*velocityOffset += (m_EmitterPos - m_PreviousEmitterPos) * invDeltaTime * m_EmitterVelocityScale;
	}
	else {
		localRotation->SetIdentity ();
			
		*velocityOffset  = m_LocalVelocity;
		*velocityOffset += t.InverseTransformDirection (m_WorldVelocity);
	}
}

template<class TransferFunction>
void ParticleEmitter::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion(2);
	transfer.Transfer (m_Enabled, "m_Enabled", kHideInEditorMask);
	TRANSFER_SIMPLE (m_Emit);
	
	transfer.Align();
	transfer.Transfer (m_MinSize, "minSize", kSimpleEditorMask);
	transfer.Transfer (m_MaxSize, "maxSize", kSimpleEditorMask);
	// WARNING: energy/emission might be inf
	// when using fastmath on ps3 or equivalent on other platform - these will be handled incorrectly
	// in that case we need to patch them on build here
	transfer.Transfer (m_MinEnergy, "minEnergy", kSimpleEditorMask);
	transfer.Transfer (m_MaxEnergy, "maxEnergy", kSimpleEditorMask);
	transfer.Transfer (m_MinEmission, "minEmission", kSimpleEditorMask);
	transfer.Transfer (m_MaxEmission, "maxEmission", kSimpleEditorMask);
	transfer.Transfer (m_WorldVelocity, "worldVelocity", kSimpleEditorMask);
	transfer.Transfer (m_LocalVelocity, "localVelocity", kSimpleEditorMask);
	transfer.Transfer (m_RndVelocity, "rndVelocity", kSimpleEditorMask);
	transfer.Transfer (m_EmitterVelocityScale, "emitterVelocityScale");
	// Emitter velocity scale was not frame rate independent!
	if (transfer.IsOldVersion(1))
		m_EmitterVelocityScale /= 40.0F;
	
	transfer.Transfer (m_TangentVelocity, "tangentVelocity");
	transfer.Transfer (m_AngularVelocity, "angularVelocity", kSimpleEditorMask);
	transfer.Transfer (m_RndAngularVelocity, "rndAngularVelocity", kSimpleEditorMask);
	transfer.Transfer (m_RndInitialRotations, "rndRotation", kSimpleEditorMask);
	transfer.Transfer (m_UseWorldSpace, "Simulate in Worldspace?");
	transfer.Transfer (m_OneShot, "m_OneShot");
}

void ParticleEmitter::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	if (IsActive())
		ResetEmitterPos();
	if (m_OneShot)
		ClearParticles ();
	UpdateManagerState( IsActive() );
}

void ParticleEmitter::ReadParticles( SimpleParticle* __restrict particle, int base, int count ) const
{
	if (base < 0 || base + count > m_Particles.size())
	{
		ErrorString("Reading out of bounds particles");
		return;
	}
	
	count += base;
	for (int i=0;i<count;++i)
	{
		SimpleParticle& output = particle[i]; 
		const Particle& input = m_Particles[i + base]; 
		output.position = input.position;
		output.velocity = input.velocity;
		output.size = input.size;
		output.rotation = Rad2Deg (input.rotation);
		output.angularVelocity = Rad2Deg (input.angularVelocity);
		output.energy =input.energy;
		output.startEnergy = input.startEnergy;
		output.color = input.color;

		Prefetch(&particle[i] + 8);
		Prefetch(&particle[i + base] + 8);
	}
}

void ParticleEmitter::WriteParticles( const SimpleParticle* __restrict particle, /*int base,*/ int count )
{
	if (count > kMaxParticleCount)
	{
		ErrorString(Format("You are assigning more than %d particles", kMaxParticleCount));
		count = kMaxParticleCount;
	}
	
	MinMaxAABB& aabb = m_PrivateInfo.aabb;
	aabb.Init();
	
	m_Particles.resize(count);

	int newSize = 0;
	for (int i=0;i<count;++i)
	{
		const SimpleParticle& input = particle[i]; 
		Particle& output = m_Particles[newSize]; 
		
		output.position = input.position;
		aabb.Encapsulate( input.position );
		output.velocity = input.velocity;
		output.size = input.size;
		output.rotation = Deg2Rad(input.rotation);
		output.angularVelocity = Deg2Rad(input.angularVelocity);
		output.energy = input.energy;
		output.startEnergy = FloatMax(input.startEnergy, input.energy);
		output.color = input.color;

		if (output.energy > 0.0f )
			newSize++;

		Prefetch(&particle[i] + 8);
	}

	m_Particles.resize(newSize);
}


void ParticleEmitter::InitParticleEnergy(Rand& r, Particle& p, float dt)
{
	// if any if energies is infinity - both energy and startEnergy will be infinity too.
	// that would be fine, but we're not quite ready to handle Inf properly


	// add deltatime - it will be removed in particle animator
	p.startEnergy	= clamp ( RangedRandom (r, m_MinEnergy, m_MaxEnergy), 0.0f, kMaxEnergy );
	p.energy		= p.startEnergy + dt + kTimeEpsilon;
}

static void ResetRandSeedForEmitter ()
{
	gEmitterRand.SetSeed (5);
}

void ParticleEmitter::InitializeClass ()
{
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Register(ResetRandSeedForEmitter);
}

void ParticleEmitter::CleanupClass ()
{
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Unregister(ResetRandSeedForEmitter);
}

IMPLEMENT_CLASS_HAS_INIT (ParticleEmitter)
IMPLEMENT_OBJECT_SERIALIZE (ParticleEmitter)
INSTANTIATE_TEMPLATE_TRANSFER (ParticleEmitter)
