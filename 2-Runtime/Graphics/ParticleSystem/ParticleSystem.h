#ifndef SHURIKEN_H
#define SHURIKEN_H

#include "Runtime/BaseClasses/GameObject.h"
#include "ParticleSystemParticle.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Modules/InitialModule.h"
#include "Modules/ShapeModule.h"
#include "Modules/EmissionModule.h"

struct ParticleSystemEmitReplay;
struct ParticleSystemState;
class ParticleSystemModule;
class SizeModule;
class RotationModule;
class ColorModule;
class UVModule;
class VelocityModule;
class ForceModule;
class ExternalForcesModule;
class ClampVelocityModule;
class SizeBySpeedModule;
class RotationBySpeedModule;
class ColorBySpeedModule;
class CollisionModule;
class SubModule;

enum ParticleSystemSimulationSpace {
	kSimLocal = 0,
	kSimWorld = 1,
};

// TODO: rename
struct ParticleSystemThreadScratchPad
{
	ParticleSystemThreadScratchPad ()
	: deltaTime (1.0f)
	{}
	
	float deltaTime;
};

enum { kGoodQualityDelay = 0, kMediumQualityDelay = 0, kLowQualityDelay = 4 }; 
struct RayBudgetState
{
	void SetQuality(int quality)
	{
		if ( m_Quality != quality )
		{
			switch (quality)
			{
			case 0:
				m_QualityFrameDelay = kGoodQualityDelay;
				break;
			case 1:
				m_QualityFrameDelay = kMediumQualityDelay;
				break;
			case 2:
				m_QualityFrameDelay = kLowQualityDelay;
				break;
			default:
				m_QualityFrameDelay = kGoodQualityDelay;
			}
			m_FramesRemaining = m_QualityFrameDelay;
			m_Quality = quality;
		};
	}
	RayBudgetState() { m_Quality=m_QualityFrameDelay=m_FramesRemaining=0; }
	bool ReceiveRays() const { return m_FramesRemaining==0; }
	void Update() { m_FramesRemaining = ( m_FramesRemaining ? m_FramesRemaining-1 : m_QualityFrameDelay ); }
	int m_Quality;
	int m_QualityFrameDelay;
	int m_FramesRemaining;
};

class ParticleSystem : public Unity::Component
{
public:
	REGISTER_DERIVED_CLASS (ParticleSystem, Unity::Component)
	DECLARE_OBJECT_SERIALIZE (ParticleSystem)

	enum
	{
		kParticleBuffer0,
#if UNITY_EDITOR // Double buffered + interpolation
		kParticleBuffer1,
#endif
		kNumParticleBuffers,
	};
	
	ParticleSystem (MemLabelId label, ObjectCreationMode mode);

	// ParticleSystem (); declared-by-macro

	void SmartReset ();
	void AddParticleSystemRenderer ();

	void Deactivate (DeactivateOperation operation);
	void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	
	#if UNITY_EDITOR
	// ParticleSystemRenderer always goes with ParticleSystem
	virtual int GetCoupledComponentClassID() const { return ClassID(ParticleSystemRenderer); }
	#endif
	
	
	static void* UpdateFunction (void* rawData);
	
	static void BeginUpdateAll();
	static void EndUpdateAll();
	static void Update(ParticleSystem& system, float deltaTime, bool fixedTimeStep, bool useProcedural, int rayBudget = 0);
	
	static bool SystemWannaRayCast(const ParticleSystem& system);
	static bool SystemWillRayCast(const ParticleSystem& system);
	static void AssignRayBudgets();

	static void SyncJobs();

	static void Emit(ParticleSystem& system, const SubEmitterEmitCommand& command, ParticleSystemEmitMode emitMode);
	
	// Client interface
	void Simulate (float t, bool restart);				// Fastforwards the particle system by simulating particles over given period of time, then pauses it.
	void Play (bool autoPrewarm = true);
	void Stop ();
	void Pause ();
	void Emit (int count);
	void EmitParticleExternal(ParticleSystemParticle* particle);
	void Clear (bool updateBounds = true);
	void AutoPrewarm();
	
	bool IsAlive () const;
	bool IsPlaying () const;
	bool IsPaused () const;
	bool IsStopped () const;
	float GetStartDelay() const;	
	void SetStartDelay(float value);
	bool GetLoop () const;
	void SetLoop (bool loop);
	bool GetPlayOnAwake () const;
	void SetPlayOnAwake (bool playOnAwake);
	ParticleSystemSimulationSpace GetSimulationSpace () const;
	void SetSimulationSpace (ParticleSystemSimulationSpace simulationSpace);
	float GetSecPosition () const;
	void SetSecPosition (float pos);
	float GetLengthInSec () const;
	void SetPlaybackSpeed (float speed);
	float GetPlaybackSpeed () const;
	void SetRayBudget (int rayBudget);
	int GetRayBudget() const;

	bool GetEnableEmission() const;
	void SetEnableEmission(bool value);
	float GetEmissionRate() const;
	void SetEmissionRate(float value);
	float GetStartSpeed() const;
	void SetStartSpeed(float value);
	float GetStartSize() const;
	void SetStartSize(float value);
	ColorRGBAf GetStartColor() const;
	void SetStartColor(ColorRGBAf value);
	float GetStartRotation() const;
	void SetStartRotation(float value);
	float GetStartLifeTime() const;
	void SetStartLifeTime(float value);
	float GetGravityModifier() const;
	void SetGravityModifier(float value);
	UInt32 GetRandomSeed() const;
	void SetRandomSeed(UInt32 value);
	int GetMaxNumParticles() const;
	void SetMaxNumParticles(int value);

	ShapeModule& GetShapeModule () { return m_ShapeModule; }
	
	
	Matrix4x4f GetLocalToWorldMatrix() const;

	void GetNumTiles(int& uvTilesX, int& uvTilesY) const;
	
	void AllocateAllStructuresOfArrays();
	void SetParticlesExternal (ParticleSystemParticle* particles, int size);
	void GetParticlesExternal (ParticleSystemParticle* particles, int size);

	int GetSafeCollisionEventSize () const;
	int GetCollisionEventsExternal (int instanceID, MonoParticleCollisionEvent* collisionEvents, int size) const;

	ParticleSystemParticles& GetParticles (int index = -1);
	const ParticleSystemParticles& GetParticles (int index = -1) const;
	size_t GetParticleCount () const;

	ParticleSystemThreadScratchPad& GetThreadScratchPad () { return m_ThreadScratchpad; }

	static void InitializeClass ();
	static void CleanupClass () {};

	void DidModifyMesh ();
	void DidDeleteMesh ();

#if UNITY_EDITOR
	void TransformChanged();
#endif
	
	void RendererBecameVisible();
	void RendererBecameInvisible();

	static size_t EmitFromData (ParticleSystemEmissionState& emissionState, size_t& numContinuous, const ParticleSystemEmissionData& emissionData, const Vector3f velocity, float fromT, float toT, float dt, float length);
private:
	static void Update0 (ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, float dt, bool fixedTimeStep);
	static void Update1 (ParticleSystem& system, ParticleSystemParticles& ps, float dt, bool fixedTimeStep, bool useProcedural, int rayBudget = 0);
	static void Update2 (ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, bool fixedTimeStep);
	static void Update1Incremental(ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, size_t fromIndex, float dt, bool useProcedural);

	static size_t EmitFromModules (const ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemEmissionState& emissionState, size_t& numContinuous, const Vector3f velocity, float fromT, float toT, float dt);
	static void StartModules (ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, const ParticleSystemEmissionState& emissionState, Vector3f initialVelocity, const Matrix4x4f& matrix, ParticleSystemParticles& ps, size_t fromIndex, float dt, float t, size_t numContinuous, float frameOffset);
	static void StartParticles(ParticleSystem& system, ParticleSystemParticles& ps, const float prevT, const float t, const float dt, const size_t numContinuous, size_t amountOfParticlesToEmit, float frameOffset);
	static void StartParticlesProcedural(ParticleSystem& system, ParticleSystemParticles& ps, const float prevT, const float t, const float dt, const size_t numContinuous, size_t amountOfParticlesToEmit, float frameOffset);
	static void UpdateProcedural(ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps);
	static void UpdateModulesPreSimulationIncremental (const ParticleSystem& system, const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, const size_t toIndex, float dt);
	static void UpdateModulesPostSimulationIncremental (const ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, float dt);
	static void UpdateModulesIncremental (const ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, size_t fromIndex, float dt);
	static void UpdateModulesNonIncremental (const ParticleSystem& system, const ParticleSystemParticles& ps, ParticleSystemParticlesTempData& psTemp, size_t fromIndex, size_t toIndex);
	static void SimulateParticles (const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, float dt);
	static void PlaybackSubEmitterCommandBuffer(const ParticleSystem& shuriken, ParticleSystemState& state, bool fixedTimeStep);
	static void UpdateBounds(const ParticleSystem& system, const ParticleSystemParticles& ps, ParticleSystemState& state);

	static void AddStagingBuffer(ParticleSystem& system);
	static int CalculateMaximumSubEmitterEmitCount(ParticleSystem& shuriken, ParticleSystemState& state, float deltaTime, bool fixedTimeStep);
	static int SetupSubEmitters(ParticleSystem& shuriken, ParticleSystemState& state);
	
	static bool CheckSupportsProcedural(const ParticleSystem& system);
	static bool DetermineSupportsProcedural(const ParticleSystem& system);
	
	bool ComputePrewarmStartParameters(float& prewarmTime, float t);

	void SetUsesAxisOfRotation();
	void SetUsesEmitAccumulator(int numAccumulators);
	void SetUsesRotationalSpeed();
	void KeepUpdating();
	void Cull();

	size_t AddNewParticles(ParticleSystemParticles& particles, size_t newParticles) const;	
	size_t LimitParticleCount(size_t requestSize) const;
	float CalculateSubEmitterMaximumLifeTime(float parentLifeTime) const;

	bool GetIsDistanceEmitter() const;
	
	void AddToManager();
	void RemoveFromManager();
	
	ParticleSystemParticles			m_Particles[kNumParticleBuffers];
	ParticleSystemParticles			m_ParticlesStaging; // staging buffer for emitting into the emitter 
	ParticleSystemReadOnlyState*		m_ReadOnlyState;
	ParticleSystemState*				m_State;
	InitialModule				m_InitialModule;
	ShapeModule					m_ShapeModule;
	EmissionModule				m_EmissionModule;

	// Dependent on energy value
	SizeModule*					m_SizeModule;
	RotationModule*				m_RotationModule; // @TODO: Requires outputs angular velocity and thus requires integration (Inconsistent with other modules in this group)
	ColorModule*				m_ColorModule;
	UVModule*					m_UVModule;
	
	// Dependent on energy value
	VelocityModule*				m_VelocityModule;
	ForceModule*				m_ForceModule;
	ExternalForcesModule*		m_ExternalForcesModule;
	
	// Depends on velocity and modifies velocity
	ClampVelocityModule*		m_ClampVelocityModule;
	
	// Dependent on velocity value
	SizeBySpeedModule*		m_SizeBySpeedModule;
	RotationBySpeedModule*	m_RotationBySpeedModule;
	ColorBySpeedModule*		m_ColorBySpeedModule;
	
	// Dependent on a position and velocity
	CollisionModule*		m_CollisionModule;
	
	SubModule*					m_SubModule;

	ParticleSystemThreadScratchPad    m_ThreadScratchpad;
	
	RayBudgetState			m_RayBudgetState;
	int						m_RayBudget;

private:
	int m_EmittersIndex;

#if UNITY_EDITOR
public:
	int m_EditorRandomSeedIndex;
	ListNode<ParticleSystem>	m_EditorListNode;
	friend class ParticleSystemEditor;
#endif
	friend class ParticleSystemRenderer;
};

#endif // SHURIKEN_H
