#include "UnityPrefix.h"
#include "InitialModule.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemUtils.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Interfaces/IPhysics.h"
#include "Runtime/BaseClasses/IsPlaying.h"

InitialModule::InitialModule () : ParticleSystemModule(true)
,	m_GravityModifier(0.0f)
,	m_InheritVelocity(0.0f)
,	m_MaxNumParticles(1000)
{
}

Vector3f InitialModule::GetGravity (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state) const
{
#if ENABLE_PHYSICS
	IPhysics* physicsModule = GetIPhysics();
	if (!physicsModule)
		return Vector3f::zero;

	Vector3f gravity = m_GravityModifier * physicsModule->GetGravity ();
	if(roState.useLocalSpace)
	{
		Matrix4x4f worldToLocal;
		Matrix4x4f::Invert_General3D(state.localToWorld, worldToLocal);
		gravity = worldToLocal.MultiplyVector3(gravity);
	}
	return gravity;
#else
	return Vector3f::zero;
#endif
}

void InitialModule::Start (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const Matrix4x4f& matrix, size_t fromIndex, float t)
{
	DebugAssert(roState.lengthInSec > 0.0001f);	
	const float normalizedT = t / roState.lengthInSec;
	DebugAssert (normalizedT >= 0.0f);
	DebugAssert (normalizedT <= 1.0f);
	
	Rand& random = GetRandom();
	
	Vector3f origin = matrix.GetPosition ();

	const size_t count = ps.array_size ();
	for (size_t q = fromIndex; q < count; ++q)
	{
		UInt32 randUInt32 = random.Get ();
		float rand = Rand::GetFloatFromInt (randUInt32);
		UInt32 randByte = Rand::GetByteFromInt (randUInt32);

		const ColorRGBA32 col = Evaluate (m_Color, normalizedT, randByte);
		float sz = std::max<float> (0.0f, Evaluate (m_Size, normalizedT, rand));
		Vector3f vel = matrix.MultiplyVector3 (Vector3f::zAxis);
		float ttl = std::max<float> (0.0f, Evaluate (m_Lifetime, normalizedT, rand));
		float rot = Evaluate (m_Rotation, normalizedT, rand);

		ps.position[q] = origin;
		ps.velocity[q] = vel;
		ps.animatedVelocity[q] = Vector3f::zero;
		ps.lifetime[q] = ttl;
		ps.startLifetime[q] = ttl;
		ps.size[q] = sz;
		ps.rotation[q] = rot;
		if(ps.usesRotationalSpeed)
			ps.rotationalSpeed[q] = 0.0f;
		ps.color[q] = col;
		ps.randomSeed[q] = random.Get(); // One more iteration to avoid visible patterns between random spawned parameters and those used in update
		if(ps.usesAxisOfRotation)
			ps.axisOfRotation[q] = Vector3f::zAxis;
		for(int acc = 0; acc < ps.numEmitAccumulators; acc++)
			ps.emitAccumulator[acc][q] = 0.0f;

	}
}

void InitialModule::Update (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, float dt) const
{
	Vector3f gravityDelta = GetGravity(roState, state) * dt;
	if(!CompareApproximately(gravityDelta, Vector3f::zero, 0.0001f))
		for (size_t q = fromIndex; q < toIndex; ++q)
			ps.velocity[q] += gravityDelta;

	for (size_t q = fromIndex; q < toIndex; ++q)
		ps.animatedVelocity[q] = Vector3f::zero;

	if(ps.usesRotationalSpeed)
		for (size_t q = fromIndex; q < toIndex; ++q)
			ps.rotationalSpeed[q] = 0.0f;
}

void InitialModule::GenerateProcedural (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const ParticleSystemEmitReplay& emit)
{
	size_t count = emit.particlesToEmit;
	float t = emit.t; 
	float alreadyPassedTime = emit.aliveTime;
	
	DebugAssert(roState.lengthInSec > 0.0001f);
	const float normalizedT = t / roState.lengthInSec;
	DebugAssert (normalizedT >= 0.0f);
	DebugAssert (normalizedT <= 1.0f);
	
	Rand& random = GetRandom();
	
	const Matrix4x4f localToWorld = !roState.useLocalSpace ? state.localToWorld : Matrix4x4f::identity;  
	Vector3f origin = localToWorld.GetPosition ();
	for (size_t i = 0; i < count; ++i)
	{
		UInt32 randUInt32 = random.Get ();
		float rand = Rand::GetFloatFromInt (randUInt32);
		UInt32 randByte = Rand::GetByteFromInt (randUInt32);

		float frameOffset = (float(i) + emit.emissionOffset) * emit.emissionGap * float(i < emit.numContinuous);
		
		const ColorRGBA32 col = Evaluate (m_Color, normalizedT, randByte);
		float sz = std::max<float> (0.0f, Evaluate (m_Size, normalizedT, rand));
		Vector3f vel = localToWorld.MultiplyVector3 (Vector3f::zAxis);
		float ttlStart = std::max<float> (0.0f, Evaluate (m_Lifetime, normalizedT, rand));
		float ttl = ttlStart - alreadyPassedTime - frameOffset;
		float rot = Evaluate (m_Rotation, normalizedT, rand);

		if (ttl < 0.0F)
			continue;

		size_t q = ps.array_size();
		ps.array_resize(ps.array_size() + 1);

		ps.position[q] = origin;
		ps.velocity[q] = vel;
		ps.animatedVelocity[q] = Vector3f::zero;
		ps.lifetime[q] = ttl;
		ps.startLifetime[q] = ttlStart;
		ps.size[q] = sz;
		ps.rotation[q] = rot;
		if(ps.usesRotationalSpeed)
			ps.rotationalSpeed[q] = 0.0f;
		ps.color[q] = col;
		ps.randomSeed[q] = random.Get(); // One more iteration to avoid visible patterns between random spawned parameters and those used in update
		if(ps.usesAxisOfRotation)
			ps.axisOfRotation[q] = Vector3f::zAxis;
		for(int acc = 0; acc < ps.numEmitAccumulators; acc++)
			ps.emitAccumulator[acc][q] = 0.0f;
	}
}

void InitialModule::CheckConsistency ()
{
	m_Lifetime.SetScalar(clamp<float> (m_Lifetime.GetScalar(), 0.05f, 100000.0f));
	m_Size.SetScalar(std::max<float> (0.0f, m_Size.GetScalar()));
	m_MaxNumParticles = std::max<int> (0, m_MaxNumParticles);
}

void InitialModule::AwakeFromLoad (ParticleSystem* system, const ParticleSystemReadOnlyState& roState)
{
	ResetSeed(roState);
}

void InitialModule::ResetSeed(const ParticleSystemReadOnlyState& roState)
{
	if(roState.randomSeed == 0)
		m_Random.SetSeed(GetGlobalRandomSeed ());
	else
		m_Random.SetSeed(roState.randomSeed);
}

Rand& InitialModule::GetRandom()
{
#if UNITY_EDITOR
	if(!IsWorldPlaying())
		return m_EditorRandom;
	else
#endif
		return m_Random;
}

template<class TransferFunction>
void InitialModule::Transfer (TransferFunction& transfer)
{
	SetEnabled(true); // always enabled
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_Lifetime, "startLifetime");
	transfer.Transfer (m_Speed, "startSpeed");
	transfer.Transfer (m_Color, "startColor");
	transfer.Transfer (m_Size, "startSize");
	transfer.Transfer (m_Rotation, "startRotation");
	transfer.Transfer (m_GravityModifier, "gravityModifier");
	transfer.Transfer (m_InheritVelocity, "inheritVelocity");
	transfer.Transfer (m_MaxNumParticles, "maxNumParticles");
}
INSTANTIATE_TEMPLATE_TRANSFER(InitialModule)
