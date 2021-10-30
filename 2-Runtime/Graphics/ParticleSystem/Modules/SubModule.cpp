#include "UnityPrefix.h"
#include "SubModule.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "../ParticleSystemUtils.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h" // Only because of PPtr comparison

bool IsUsingSubEmitter(ParticleSystem* emitter)
{
	const void* shurikenSelf = NULL;
	return emitter != shurikenSelf;
}

SubModule::SubModule () : ParticleSystemModule(false)
{
}

void SubModule::Update (const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, size_t fromIndex, size_t toIndex, float dt) const
{
	Assert(state.numCachedSubDataBirth <= kParticleSystemMaxNumEmitAccumulators);
	for(int i = 0; i < state.numCachedSubDataBirth; i++)
	{
		const ParticleSystemSubEmitterData& data = state.cachedSubDataBirth[i];
		//const bool culled = (data.maxLifetime < state.accumulatedDt);
		const float startDelay = data.startDelayInSec;
		const float length = data.lengthInSec;
		for (int q = fromIndex; q < toIndex; ++q)
		{
			const float t = std::max(0.0f, ps.startLifetime[q] - ps.lifetime[q] - dt) - startDelay;
			const bool started = (t >= 0.0f);
			const bool ended = (t >= length);
			if(started && !ended)
			{
				ParticleSystemEmissionState emissionState;
				emissionState.m_ToEmitAccumulator = ps.emitAccumulator[i][q];
				RecordEmit(emissionState, data, roState, state, ps, kParticleSystemSubTypeBirth, i, q, t, dt, length);
				ps.emitAccumulator[i][q] = emissionState.m_ToEmitAccumulator;
			}
		}
	}
}

void SubModule::RemoveDuplicatePtrs (ParticleSystem** shurikens)
{
	for(int i = 0; i < kParticleSystemMaxSubTotal-1; i++)
		for(int j = i+1; j < kParticleSystemMaxSubTotal; j++)
			if(shurikens[i] && (shurikens[i] == shurikens[j]))
				shurikens[i] = NULL;
}

// This can not be cached between frames since the referenced particle systems might be deleted at any point.
int SubModule::GetSubEmitterPtrs (ParticleSystem** subEmitters) const
{
	for(int i = 0; i < kParticleSystemMaxSubTotal; i++)
		subEmitters[i] = NULL;

	int numSubEmitters = 0;
	for(int i = 0; i < kParticleSystemMaxSubBirth; i++)
	{
		ParticleSystem* subParticleSystem = m_SubEmittersBirth[i];
		if (IsUsingSubEmitter(subParticleSystem))
			subEmitters[numSubEmitters++] = subParticleSystem;
	}
	for(int i = 0; i < kParticleSystemMaxSubCollision; i++)
	{
		ParticleSystem* subParticleSystem = m_SubEmittersCollision[i];
		if (IsUsingSubEmitter(subParticleSystem))
			subEmitters[numSubEmitters++] = subParticleSystem;
	}
	for(int i = 0; i < kParticleSystemMaxSubDeath; i++)
	{
		ParticleSystem* subParticleSystem = m_SubEmittersDeath[i];
		if (IsUsingSubEmitter(subParticleSystem))
			subEmitters[numSubEmitters++] = subParticleSystem;
	}
	return numSubEmitters;
}

int SubModule::GetSubEmitterPtrsBirth (ParticleSystem** subEmitters) const
{
	int numSubEmitters = 0;
	for(int i = 0; i < kParticleSystemMaxSubBirth; i++)
	{
		ParticleSystem* subParticleSystem = m_SubEmittersBirth[i];
		if (IsUsingSubEmitter(subParticleSystem))
			subEmitters[numSubEmitters++] = subParticleSystem;
	}
	return numSubEmitters;
}

int SubModule::GetSubEmitterPtrsCollision (ParticleSystem** subEmitters) const
{
	int numSubEmitters = 0;
	for(int i = 0; i < kParticleSystemMaxSubCollision; i++)
	{
		ParticleSystem* subParticleSystem = m_SubEmittersCollision[i];
		if (IsUsingSubEmitter(subParticleSystem))
			subEmitters[numSubEmitters++] = subParticleSystem;
	}
	return numSubEmitters;
}

int SubModule::GetSubEmitterPtrsDeath(ParticleSystem** subEmitters) const
{
	int numSubEmitters = 0;
	for(int i = 0; i < kParticleSystemMaxSubDeath; i++)
	{
		ParticleSystem* subParticleSystem = m_SubEmittersDeath[i];
		if (IsUsingSubEmitter(subParticleSystem))
			subEmitters[numSubEmitters++] = subParticleSystem;
	}
	return numSubEmitters;
}

int SubModule::GetSubEmitterTypeCount(ParticleSystemSubType type) const
{
	int count = 0;
	const void* shurikenSelf = NULL;
	if(type == kParticleSystemSubTypeBirth) {
		for(int i = 0; i < kParticleSystemMaxSubBirth; i++)
			if(m_SubEmittersBirth[i] != shurikenSelf)
				count++;
	} else if(type == kParticleSystemSubTypeCollision) {
		for(int i = 0; i < kParticleSystemMaxSubCollision; i++)
			if(m_SubEmittersCollision[i] != shurikenSelf)
				count++;
	} else if(type == kParticleSystemSubTypeDeath) {
		for(int i = 0; i < kParticleSystemMaxSubDeath; i++)
			if(m_SubEmittersDeath[i] != shurikenSelf)
				count++;
	} else {
		Assert(!"Sub emitter type not implemented.");
	}
	return count;
}

template<class TransferFunction>
void SubModule::Transfer (TransferFunction& transfer)
{
	ParticleSystemModule::Transfer (transfer);
	transfer.Transfer (m_SubEmittersBirth[0], "subEmitterBirth");
	transfer.Transfer (m_SubEmittersBirth[1], "subEmitterBirth1");
	transfer.Transfer (m_SubEmittersCollision[0], "subEmitterCollision");
	transfer.Transfer (m_SubEmittersCollision[1], "subEmitterCollision1");
	transfer.Transfer (m_SubEmittersDeath[0], "subEmitterDeath");
	transfer.Transfer (m_SubEmittersDeath[1], "subEmitterDeath1");
}
INSTANTIATE_TEMPLATE_TRANSFER(SubModule)
