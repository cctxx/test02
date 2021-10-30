#include "UnityPrefix.h"
#include "ParticleSystemUtils.h"
#include "ParticleSystem.h"
#include "ParticleSystemCurves.h"
#include "Modules/ParticleSystemModule.h"
#include "Runtime/BaseClasses/IsPlaying.h"

#if UNITY_EDITOR
#include "Editor/Src/ParticleSystem/ParticleSystemEditor.h"
#endif

UInt32 randomSeed = 0x1337;

UInt32 GetGlobalRandomSeed ()
{
	return ++randomSeed;
}

void ResetGlobalRandomSeed ()
{
	randomSeed = 0x1337;
}

Vector2f CalculateInverseLerpOffsetScale (const Vector2f& range)
{
	Assert (range.x < range.y);
	float scale = 1.0F / (range.y - range.x);
	return Vector2f (scale, -range.x * scale);
}

void CalculatePositionAndVelocity(Vector3f& initialPosition, Vector3f& initialVelocity, const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, const ParticleSystemParticles& ps, const size_t index)
{
	initialPosition = ps.position[index];
	initialVelocity = ps.velocity[index] + ps.animatedVelocity[index];
	if(roState.useLocalSpace)
	{
		// If we are in local space, transform to world space to make independent of this emitters transform
		initialPosition = state.localToWorld.MultiplyPoint3(initialPosition);
		initialVelocity = state.localToWorld.MultiplyVector3(initialVelocity);
	}
}

void KillParticle(const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, size_t index, size_t& particleCount)
{
	Assert(particleCount > 0);

	for(int i = 0; i < state.numCachedSubDataDeath; i++)
	{
		ParticleSystemEmissionState emissionState;
		RecordEmit(emissionState, state.cachedSubDataDeath[i], roState, state, ps, kParticleSystemSubTypeDeath, i, index, 0.0f, 0.0001f, 1.0f);
	}

	ps.element_assign (index, particleCount - 1);
	--particleCount;

}

void RecordEmit(ParticleSystemEmissionState& emissionState, const ParticleSystemSubEmitterData& subEmitterData, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, ParticleSystemSubType type, int subEmitterIndex, size_t particleIndex, float t, float dt, float length)
{	
	size_t numContinuous = 0;
	Vector3f initialPosition;
	Vector3f initialVelocity;
	CalculatePositionAndVelocity(initialPosition, initialVelocity, roState, state, ps, particleIndex);
	int amountOfParticlesToEmit = ParticleSystem::EmitFromData (emissionState, numContinuous, subEmitterData.emissionData, initialVelocity, t, std::min(t + dt, length), dt, length);
	if(amountOfParticlesToEmit)
	{
		if(!state.recordSubEmits)
			ParticleSystem::Emit(*subEmitterData.emitter, SubEmitterEmitCommand(emissionState, initialPosition, initialVelocity, type, subEmitterIndex, amountOfParticlesToEmit, numContinuous, t, dt), kParticleSystemEMStaging);
		else if(!state.subEmitterCommandBuffer.IsFull())
			state.subEmitterCommandBuffer.AddCommand(emissionState, initialPosition, initialVelocity, type, subEmitterIndex, amountOfParticlesToEmit, numContinuous, t, dt);
	}
}

bool GetTransformationMatrix(Matrix4x4f& output, const bool isSystemInWorld, const bool isCurveInWorld, const Matrix4x4f& localToWorld)
{
	if(isCurveInWorld != isSystemInWorld)
	{
		if(isSystemInWorld)
			output = localToWorld;
		else 
			Matrix4x4f::Invert_General3D(localToWorld, output);
		return true;
	}
	else
	{
		output = Matrix4x4f::identity;
		return false;
	}	
}

bool GetTransformationMatrices(Matrix4x4f& output, Matrix4x4f& outputInverse, const bool isSystemInWorld, const bool isCurveInWorld, const Matrix4x4f& localToWorld)
{
	if(isCurveInWorld != isSystemInWorld)
	{
		if(isSystemInWorld)
		{
			output = localToWorld;
			Matrix4x4f::Invert_General3D(localToWorld, outputInverse);
		}
		else 
		{
			Matrix4x4f::Invert_General3D(localToWorld, output);
			outputInverse = localToWorld;
		}
		return true;
	}
	else
	{
		output = Matrix4x4f::identity;
		outputInverse = Matrix4x4f::identity;
		return false;
	}	
}
