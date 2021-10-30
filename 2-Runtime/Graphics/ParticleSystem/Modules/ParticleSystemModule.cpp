#include "UnityPrefix.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "ParticleSystemModule.h"

ParticleSystemReadOnlyState::ParticleSystemReadOnlyState()
:	lengthInSec (5.0f)
,	startDelay (0.0f)
,	speed (1.0f)
,	randomSeed (0)
,	looping (true)
,	prewarm (false)
,	playOnAwake (true)
,	useLocalSpace (true)
{
}

void ParticleSystemReadOnlyState::CheckConsistency()
{
	lengthInSec = std::max(lengthInSec, 0.1f);
	lengthInSec = std::min(lengthInSec, 100000.0f); // Very large values can lead to editor locking up due to numerical instability.	
	startDelay = std::max(startDelay, 0.0f);
	speed = std::max(speed, 0.0f);
}

template<class TransferFunction>
void ParticleSystemReadOnlyState::Transfer (TransferFunction& transfer)
{
	TRANSFER (lengthInSec);
	TRANSFER (startDelay);
	TRANSFER (speed);
	TRANSFER (randomSeed);
	TRANSFER (looping);
	TRANSFER (prewarm);
	TRANSFER (playOnAwake);
	transfer.Transfer (useLocalSpace, "moveWithTransform");
}
INSTANTIATE_TEMPLATE_TRANSFER(ParticleSystemReadOnlyState)

ParticleSystemState::ParticleSystemState () 
:	playing (false)
,	needRestart (true)
,	stopEmitting (false)
,	accumulatedDt (0.0f)
,	delayT (0.0f)
,	t (0.0f)
,	maxSize (0.0f)
,	isSubEmitter (false)
,	recordSubEmits(false)
,	cullTime(0.0)
,	culled(false)
,	numLoops(0)
,	invalidateProcedural(false)
,	supportsProcedural(true)
,	cachedForces(0)
,	numCachedForces(0)
,	cachedSubDataBirth(0)
,	numCachedSubDataBirth(0)
,	cachedSubDataCollision(0)
,	numCachedSubDataCollision(0)
,	cachedSubDataDeath(0)
,	numCachedSubDataDeath(0)
,	cachedCollisionPlanes(0)
,	numCachedCollisionPlanes(0)
,	rayBudget(0)
,	nextParticleToTrace(0)
{
	ClearSubEmitterCommandBuffer();

	localToWorld.SetIdentity ();
	emitterVelocity = Vector3f::zero;
	emitterScale = Vector3f::one;
	minMaxAABB = MinMaxAABB (Vector3f::zero, Vector3f::zero);
}

void ParticleSystemState::Tick (const ParticleSystemReadOnlyState& constState, float dt)
{
	t += dt;

	for(int i = 0; i < subEmitterCommandBuffer.commandCount; i++)
		subEmitterCommandBuffer.commands[i].timeAlive += dt;

	if (!constState.looping)
		t = std::min<float> (t, constState.lengthInSec);
	else
		if(t > constState.lengthInSec)
		{
			t -= constState.lengthInSec;
			numLoops++;
		}
}

void ParticleSystemState::ClearSubEmitterCommandBuffer()
{
	if(cachedSubDataBirth)
	{
		for (int i = 0; i < numCachedSubDataBirth; ++i)
		{
			(cachedSubDataBirth+i)->~ParticleSystemSubEmitterData();
		}
		FREE_TEMP_MANUAL(cachedSubDataBirth);
	}
	if(cachedSubDataCollision)
	{
		for (int i = 0; i < numCachedSubDataCollision; ++i)
		{
			(cachedSubDataCollision+i)->~ParticleSystemSubEmitterData();
		}
		FREE_TEMP_MANUAL(cachedSubDataCollision);
	}
	if(cachedSubDataDeath)
	{
		for (int i = 0; i < numCachedSubDataDeath; ++i)
		{
			(cachedSubDataDeath+i)->~ParticleSystemSubEmitterData();
		}
		FREE_TEMP_MANUAL(cachedSubDataDeath);
	}
	if(subEmitterCommandBuffer.commands)
		FREE_TEMP_MANUAL(subEmitterCommandBuffer.commands);

	cachedSubDataBirth = cachedSubDataCollision = cachedSubDataDeath = 0;
	numCachedSubDataBirth = numCachedSubDataCollision = numCachedSubDataDeath = 0;
	subEmitterCommandBuffer.commands = 0;
	subEmitterCommandBuffer.commandCount = subEmitterCommandBuffer.maxCommandCount = 0;
}

template<class TransferFunction>
void ParticleSystemState::Transfer (TransferFunction& transfer)
{
	TRANSFER_DEBUG (t);
}
INSTANTIATE_TEMPLATE_TRANSFER(ParticleSystemState)


template<class TransferFunction>
void ParticleSystemModule::Transfer (TransferFunction& transfer)
{
	transfer.Transfer (m_Enabled, "enabled"); transfer.Align ();
}
INSTANTIATE_TEMPLATE_TRANSFER(ParticleSystemModule)
