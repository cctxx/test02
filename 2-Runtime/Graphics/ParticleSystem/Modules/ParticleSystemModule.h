#ifndef SHURIKENMODULE_H
#define SHURIKENMODULE_H

#include "../ParticleSystemCommon.h"
#include "../ParticleSystemCurves.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Utilities/dynamic_array.h"

#define ENABLE_MULTITHREADED_PARTICLES ENABLE_MULTITHREADED_CODE

#define DECLARE_MODULE(name) const char* GetName () { return #name; } DEFINE_GET_TYPESTRING(name)

class ParticleSystem;
class Plane;

struct ParticleSystemEmissionState
{
	ParticleSystemEmissionState() { Clear(); }
	inline void Clear()
	{
		m_ToEmitAccumulator = 0.0f;
		m_ParticleSpacing = 0.0f;
	}
	float m_ParticleSpacing;
	float m_ToEmitAccumulator;
};

struct ParticleSystemEmissionData
{
	enum { kMaxNumBursts = 4 };

	int type;
	MinMaxCurve rate;
	float burstTime[kMaxNumBursts];
	UInt16 burstParticleCount[kMaxNumBursts];
	UInt8 burstCount;
};


struct ParticleSystemSubEmitterData
{
	ParticleSystemSubEmitterData()
	:maxLifetime(0.0f)
	,startDelayInSec(0.0f)
	,lengthInSec(0.0f)
	{}

	ParticleSystemEmissionData emissionData;
	float maxLifetime;
	float startDelayInSec;
	float lengthInSec;
	ParticleSystem* emitter;
};

// @TODO: Find "pretty" place for shared structs and enums?
struct ParticleSystemEmitReplay
{
	float  t;
	float  aliveTime;
	float emissionOffset;
	float emissionGap;
	int    particlesToEmit;
	size_t numContinuous;
	UInt32 randomSeed;
	
	ParticleSystemEmitReplay (float inT, int inParticlesToEmit, float inEmissionOffset, float inEmissionGap, size_t inNumContinuous, UInt32 inRandomSeed)
		: t (inT), particlesToEmit (inParticlesToEmit), aliveTime(0.0F), emissionOffset(inEmissionOffset), emissionGap(inEmissionGap), numContinuous(inNumContinuous), randomSeed(inRandomSeed)
	{}
};

struct SubEmitterEmitCommand
{
	SubEmitterEmitCommand(ParticleSystemEmissionState inEmissionState, Vector3f inPosition, Vector3f inVelocity, ParticleSystemSubType inSubEmitterType, int inSubEmitterIndex, int inParticlesToEmit, int inParticlesToEmitContinuous, float inParentT, float inDeltaTime)
	:emissionState(inEmissionState)
	,position(inPosition)
	,velocity(inVelocity)
	,subEmitterType(inSubEmitterType)
	,subEmitterIndex(inSubEmitterIndex)
	,particlesToEmit(inParticlesToEmit)
	,particlesToEmitContinuous(inParticlesToEmitContinuous)
	,deltaTime(inDeltaTime)
	,parentT(inParentT)
	,timeAlive(0.0f)
	{
	}

	ParticleSystemEmissionState emissionState;
	Vector3f position;
	Vector3f velocity;
	ParticleSystemSubType subEmitterType;
	int subEmitterIndex;
	int particlesToEmit;
	int particlesToEmitContinuous;
	float deltaTime;
	float parentT;		// Used for StartModules
	float timeAlive;
};

struct ParticleSystemSubEmitCmdBuffer
{
	ParticleSystemSubEmitCmdBuffer()
	:commands(0)
	,commandCount(0)
	,maxCommandCount(0)
	{}
	
	inline void AddCommand(const ParticleSystemEmissionState& emissionState, const Vector3f& initialPosition, const Vector3f& initialVelocity, const ParticleSystemSubType type, const int index, const int particlesToEmit, const int particlesToEmitContinuous, const float parentT, const float dt)
	{
		commands[commandCount++] = SubEmitterEmitCommand(emissionState, initialPosition, initialVelocity, type, index, particlesToEmit, particlesToEmitContinuous, parentT, dt);
	}
	inline bool IsFull() { return commandCount >= maxCommandCount; }
	
	SubEmitterEmitCommand* commands;
	int commandCount;
	int maxCommandCount; // Mostly to assert/test against trashing memory
};

struct ParticleSystemExternalCachedForce
{
	Vector3f position;
	Vector3f direction;
	int forceType;
	float radius;
	float forceMain;
	float forceTurbulence; // not yet implemented
};

// @TODO: Maybe there's a better name for this? ParticleSystemSerializedState? Some shit like that :)
struct ParticleSystemReadOnlyState
{
	ParticleSystemReadOnlyState();
	
	void CheckConsistency();

	float lengthInSec;
	float startDelay;
	float speed;
	UInt32 randomSeed;
	bool looping;
	bool prewarm;
	bool playOnAwake;
	bool useLocalSpace;

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);
};

// Some of these aren't actually state, but more like context. Separate it?
struct ParticleSystemState
{	
	// state
	float accumulatedDt;
	float delayT;
	bool playing;
	bool needRestart;
	bool stopEmitting;
	size_t rayBudget;
	size_t nextParticleToTrace;

	bool GetIsSubEmitter() const { return isSubEmitter; }
private:
	// When setting this we need to ensure some other things happen as well
	bool isSubEmitter;

public:
	
	bool recordSubEmits;

	// Procedural mode / culling
	bool supportsProcedural;	// With the current parameter set, does the emitter support procedural mode?
	bool invalidateProcedural;  // This is set if anything changes from script at some point when running a system
	bool culled;				// Is particle system currently culled?
	double cullTime;			// Absolute time, so we need as double in case it runs for ages
	int numLoops;				// Number of loops executed
	
	// per-frame
	Matrix4x4f localToWorld;
	Matrix4x4f worldToLocal;
	Vector3f emitterVelocity;
	Vector3f emitterScale;
	
	MinMaxAABB minMaxAABB;
	float maxSize; // Maximum size of particles due to setting from script
	float t;

	// Temp alloc stuff
	ParticleSystemSubEmitterData* cachedSubDataBirth;
	size_t numCachedSubDataBirth;
	ParticleSystemSubEmitterData* cachedSubDataCollision;
	size_t numCachedSubDataCollision;
	ParticleSystemSubEmitterData* cachedSubDataDeath;
	size_t numCachedSubDataDeath;
	ParticleSystemExternalCachedForce* cachedForces;
	size_t numCachedForces;
	Plane* cachedCollisionPlanes;
	size_t numCachedCollisionPlanes;
	ParticleSystemSubEmitCmdBuffer subEmitterCommandBuffer;

	dynamic_array<ParticleSystemEmitReplay> emitReplay;
	ParticleSystemEmissionState emissionState;
	
	ParticleSystemState ();

	void Tick (const ParticleSystemReadOnlyState& constState, float dt);
	void ClearSubEmitterCommandBuffer();

	void SetIsSubEmitter(bool inIsSubEmitter)
	{
		if(inIsSubEmitter)
		{
			stopEmitting = true;
			invalidateProcedural = true;
		}
		isSubEmitter = inIsSubEmitter;
	}

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);
};


class ParticleSystemModule
{
public:
	DECLARE_SERIALIZE (ParticleSystemModule)
	ParticleSystemModule (bool enabled) : m_Enabled (enabled) {}
	virtual ~ParticleSystemModule () {}
	
	inline bool GetEnabled() const { return m_Enabled; }
	inline void SetEnabled(bool enabled) { m_Enabled = enabled; }

private:
	// shared data
	bool m_Enabled;
};

#endif // SHURIKENMODULE_H
