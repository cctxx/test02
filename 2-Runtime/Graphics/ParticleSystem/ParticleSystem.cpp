#include "UnityPrefix.h"
#include "ParticleSystem.h"
#include "ParticleSystemRenderer.h"
#include "ParticleSystemCurves.h"
#include "ParticleSystemUtils.h"
#include "Modules/ParticleSystemModule.h"
#include "Modules/InitialModule.h"
#include "Modules/ShapeModule.h"
#include "Modules/EmissionModule.h"
#include "Modules/SizeModule.h"
#include "Modules/RotationModule.h"
#include "Modules/ColorModule.h"
#include "Modules/UVModule.h"
#include "Modules/VelocityModule.h"
#include "Modules/ForceModule.h"
#include "Modules/ExternalForcesModule.h"
#include "Modules/ClampVelocityModule.h"
#include "Modules/SizeByVelocityModule.h"
#include "Modules/RotationByVelocityModule.h"
#include "Modules/ColorByVelocityModule.h"
#include "Modules/CollisionModule.h"
#include "Modules/SubModule.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Math/Random/rand.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Threads/JobScheduler.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Misc/QualitySettings.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

#if UNITY_EDITOR
#include "Editor/Src/ParticleSystem/ParticleSystemEditor.h"
#include "Runtime/Mono/MonoManager.h"
#endif

// P1:
// . Calling ps.Play() in Start(). Does it actually stsrt playing when game starts? No.

// P2:
// Automatic Culling:
// . Improve procedural AABB
//   . Gravity: make it work with transforming into local space the same way velocity and force modules work
//   . Get tighter bounds by forces counteracting eachother instead of always expanding
//   . For procedural systems, no gain in calculating AABB every frame. Just do it when something changes.
// . Solving for roots analytically
//   . http://en.wikipedia.org/wiki/Cubic_function
//   . http://en.wikipedia.org/wiki/Quartic_function
//   . http://en.wikipedia.org/wiki/Quintic_function

// External Forces:
// . Currently not using turbulence. Start using that?

// Other:
// . Batching: Make it work with meshes as well
// . !!! Add runtime tests for playback code. Playing, stopping, simulating etc. Make sure it's 100%.

struct ParticleSystemManager
{
	ParticleSystemManager()
	:needSync(false)
	{
		activeEmitters.reserve(32);
	}

	dynamic_array<ParticleSystem*> activeEmitters;

#if ENABLE_MULTITHREADED_PARTICLES
	JobScheduler::JobGroupID worldCollisionJobGroup;	// group for particle systems performing world collisions, PhysX is currently not threadsafe so deleting objects in LateUpdate could cause crashes as that could overlap with collision testing
	JobScheduler::JobGroupID jobGroup;					// for any other collisions
#endif
	bool needSync;
};

Material* GetDefaultParticleMaterial ()
{
	#if WEBPLUG
	if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion_OldWebResourcesAdded))
		return GetBuiltinOldWebResource<Material> ("Default-Particle.mat");
	#endif

	#if UNITY_EDITOR
	return GetBuiltinExtraResource<Material> ("Default-Particle.mat");
	#endif

	// If someone happens to create a new particle system component at runtime,
	// just don't assign any material. The default one might not even be included
	// into the build.
	return NULL;
}

ParticleSystemManager gParticleSystemManager;

PROFILER_INFORMATION(gParticleSystemProfile, "ParticleSystem.Update", kProfilerParticles)
PROFILER_INFORMATION(gParticleSystemPrewarm, "ParticleSystem.Prewarm", kProfilerParticles)
PROFILER_INFORMATION(gParticleSystemWait, "ParticleSystem.WaitForUpdateThreads", kProfilerParticles)
PROFILER_INFORMATION(gParticleSystemJobProfile, "ParticleSystem.UpdateJob", kProfilerParticles)

PROFILER_INFORMATION(gParticleSystemUpdateCollisions, "ParticleSystem.UpdateCollisions", kProfilerParticles)


#define MAX_TIME_STEP (0.03f)
static float GetTimeStep(float dt, bool fixedTimeStep)
{
	if(fixedTimeStep)
		return GetTimeManager().GetFixedDeltaTime();
	else if(dt > MAX_TIME_STEP)
		return dt / Ceilf(dt / MAX_TIME_STEP);
	else
		return dt;
}

static void ApplyStartDelay (float& delayT, float& accumulatedDt)
{
	if(delayT > 0.0f)
	{
		delayT -= accumulatedDt;
		accumulatedDt = max(-delayT, 0.0f);
		delayT = max(delayT, 0.0f);
	}
	DebugAssert(delayT >= 0.0f);
}

static inline void CheckParticleConsistency(ParticleSystemState& state, ParticleSystemParticle& particle)
{
	particle.lifetime = min(particle.lifetime, particle.startLifetime);
	state.maxSize = max(state.maxSize, particle.size);
}

ParticleSystem::ParticleSystem (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_RayBudget(0)
,	m_EmittersIndex (-1)
#if UNITY_EDITOR
,	m_EditorListNode ( this )
#endif
{
	m_State = new ParticleSystemState ();
	m_ReadOnlyState = new ParticleSystemReadOnlyState ();

	m_SizeModule = new SizeModule ();
	m_RotationModule = new RotationModule ();
	m_ColorModule = new ColorModule ();
	m_UVModule = new UVModule ();
	m_VelocityModule = new VelocityModule ();
	m_ForceModule = new ForceModule	();
	m_ExternalForcesModule = new ExternalForcesModule ();
	m_ClampVelocityModule = new ClampVelocityModule ();
	m_SizeBySpeedModule = new SizeBySpeedModule ();
	m_RotationBySpeedModule = new RotationBySpeedModule ();
	m_ColorBySpeedModule = new ColorBySpeedModule ();
	m_CollisionModule = new CollisionModule ();
	m_SubModule = new SubModule ();

#if UNITY_EDITOR
	ParticleSystemEditor::SetupDefaultParticleSystem(*this);
	m_EditorRandomSeedIndex = 0;
#endif
}

ParticleSystem::~ParticleSystem ()
{
	delete m_State;
	delete m_ReadOnlyState;

	delete m_SizeModule;
	delete m_RotationModule;
	delete m_ColorModule;
	delete m_UVModule;
	delete m_VelocityModule;
	delete m_ForceModule;
	delete m_ExternalForcesModule;
	delete m_ClampVelocityModule;
	delete m_SizeBySpeedModule;
	delete m_RotationBySpeedModule;
	delete m_ColorBySpeedModule;
	delete m_CollisionModule;
	delete m_SubModule;
}

void ParticleSystem::InitializeClass ()
{
#if UNITY_EDITOR
	// This is only necessary to avoid pain during development (Pre 3.5) - can be removed later...
	RegisterAllowTypeNameConversion ("MinMaxColorCurve", "MinMaxColor");
	// Only needed due to 3.5 beta content
	RegisterAllowNameConversion ("MinMaxCurve", "maxScalar", "scalar");
	RegisterAllowNameConversion ("InitialModule", "lifetime", "startLifetime");
	RegisterAllowNameConversion ("InitialModule", "speed", "startSpeed");
	RegisterAllowNameConversion ("InitialModule", "color", "startColor");
	RegisterAllowNameConversion ("InitialModule", "size", "startSize");
	RegisterAllowNameConversion ("InitialModule", "rotation", "startRotation");
	RegisterAllowNameConversion ("ShapeModule", "m_Radius", "radius");
	RegisterAllowNameConversion ("ShapeModule", "m_Angle", "angle");
	RegisterAllowNameConversion ("ShapeModule", "m_BoxX", "boxX");
	RegisterAllowNameConversion ("ShapeModule", "m_BoxY", "boxY");
	RegisterAllowNameConversion ("ShapeModule", "m_BoxZ", "boxZ");

	REGISTER_MESSAGE_VOID (ParticleSystem, kTransformChanged, TransformChanged);
#endif

	REGISTER_MESSAGE_VOID(ParticleSystem, kDidDeleteMesh, DidDeleteMesh);
	REGISTER_MESSAGE_VOID(ParticleSystem, kDidModifyMesh, DidModifyMesh);
}

void ParticleSystem::SetRayBudget (int rayBudget)
{
	m_RayBudget = rayBudget;
};

int ParticleSystem::GetRayBudget() const
{
	return m_RayBudget;
};

void ParticleSystem::DidModifyMesh ()
{
	m_ShapeModule.DidModifyMeshData();
}

void ParticleSystem::DidDeleteMesh ()
{
	m_ShapeModule.DidDeleteMesh(this);
}

void ParticleSystem::Cull()
{
	if(!IsWorldPlaying())
		return;

	m_State->culled = true;

	Clear(false);
	m_State->cullTime = GetCurTime ();
	RemoveFromManager();
}

void ParticleSystem::RendererBecameVisible()
{
	if(m_State->culled)
	{
		m_State->culled = false;
		if(!m_State->stopEmitting && IsPlaying() && IsWorldPlaying() && CheckSupportsProcedural (*this))
		{
			double timeDiff = GetCurTime() - m_State->cullTime;
			Simulate(m_State->numLoops * m_ReadOnlyState->lengthInSec + m_State->t + timeDiff, true);
			Play();
		}
	}
}

void ParticleSystem::RendererBecameInvisible()
{
	if(!m_State->culled && CheckSupportsProcedural(*this))
		Cull();
}

void ParticleSystem::AddToManager()
{
	if(m_EmittersIndex >= 0)
		return;
	size_t index = gParticleSystemManager.activeEmitters.size();
	gParticleSystemManager.activeEmitters.push_back(this);
	m_EmittersIndex = index;
}

void ParticleSystem::RemoveFromManager()
{
	if(m_EmittersIndex < 0)
		return;
	const int index = m_EmittersIndex;
	gParticleSystemManager.activeEmitters[index]->m_EmittersIndex = -1;
	gParticleSystemManager.activeEmitters[index] = gParticleSystemManager.activeEmitters.back();
	if(gParticleSystemManager.activeEmitters[index] != this) // corner case
		gParticleSystemManager.activeEmitters[index]->m_EmittersIndex = index;
	gParticleSystemManager.activeEmitters.resize_uninitialized(gParticleSystemManager.activeEmitters.size() - 1);
}

void ParticleSystem::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	m_InitialModule.AwakeFromLoad(this, *m_ReadOnlyState);
	m_ShapeModule.AwakeFromLoad(this, *m_ReadOnlyState);

	if (!IsActive () || (kDefaultAwakeFromLoad == awakeMode))
		return;

	m_State->localToWorld = GetComponent (Transform).GetLocalToWorldMatrixNoScale();
	Matrix4x4f::Invert_General3D(m_State->localToWorld, m_State->worldToLocal);
	m_State->maxSize = 0.0f;
	m_State->invalidateProcedural = false;

	if (IsWorldPlaying () && m_ReadOnlyState->playOnAwake)
		Play ();

	// Does this even happen?
	if(GetParticleCount() || IsPlaying())
		AddToManager();
}

void ParticleSystem::Deactivate (DeactivateOperation operation)
{
	Super::Deactivate(operation);
	SyncJobs();
	RemoveFromManager();
}

void ParticleSystem::SyncJobs()
{
	if(gParticleSystemManager.needSync)
	{
#if ENABLE_MULTITHREADED_PARTICLES
		PROFILER_BEGIN(gParticleSystemWait, NULL);
		JobScheduler& scheduler = GetJobScheduler();
		scheduler.WaitForGroup (gParticleSystemManager.jobGroup);
		PROFILER_END;

#endif
		const float deltaTimeEpsilon = 0.0001f;
		float deltaTime = GetDeltaTime();
		if(deltaTime < deltaTimeEpsilon)
			return;

		for(int i = 0; i < gParticleSystemManager.activeEmitters.size(); ++i)
		{
			ParticleSystem& system = *gParticleSystemManager.activeEmitters[i];
			ParticleSystemState& state = *system.m_State;
			system.Update2 (system, *system.m_ReadOnlyState, state, false);
		}

		gParticleSystemManager.needSync = false;
	}
}

#if ENABLE_MULTITHREADED_PARTICLES
void* ParticleSystem::UpdateFunction (void* rawData)
{
	Assert (rawData);
	ParticleSystem* system = reinterpret_cast<ParticleSystem*>(rawData);
	ParticleSystem::Update1 (*system, system->GetParticles((int)ParticleSystem::kParticleBuffer0), system->GetThreadScratchPad().deltaTime, false, false, system->m_RayBudget);
	return NULL;
}
#endif

#define MAX_NUM_SUB_EMIT_CMDS (1024)
void ParticleSystem::Update(ParticleSystem& system, float deltaTime, bool fixedTimeStep, bool useProcedural, int rayBudget)
{
	system.m_State->recordSubEmits = false;
	Update0 (system, *system.m_ReadOnlyState, *system.m_State, deltaTime, fixedTimeStep);
	Update1 (system, system.GetParticles((int)ParticleSystem::kParticleBuffer0), deltaTime, fixedTimeStep, useProcedural, rayBudget);
	Update2 (system, *system.m_ReadOnlyState, *system.m_State, fixedTimeStep);
#if UNITY_EDITOR
	ParticleSystemEditor::PerformInterpolationStep(&system);
#endif
}

size_t ParticleSystem::EmitFromModules (const ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemEmissionState& emissionState, size_t& numContinuous, const Vector3f velocity, float fromT, float toT, float dt)
{
	if(system.m_EmissionModule.GetEnabled())
		return EmitFromData(emissionState, numContinuous, system.m_EmissionModule.GetEmissionDataRef(), velocity, fromT, toT, dt, roState.lengthInSec);
	return 0;
}

size_t ParticleSystem::EmitFromData (ParticleSystemEmissionState& emissionState, size_t& numContinuous, const ParticleSystemEmissionData& emissionData, const Vector3f velocity, float fromT, float toT, float dt, float length)
{
	size_t amountOfParticlesToEmit = 0;
	EmissionModule::Emit(emissionState, amountOfParticlesToEmit, numContinuous, emissionData, velocity, fromT, toT, dt, length);
	return amountOfParticlesToEmit;
}

size_t ParticleSystem::LimitParticleCount(size_t requestSize) const
{
	const size_t maxNumParticles = m_InitialModule.GetMaxNumParticles();
	return min<size_t>(requestSize, maxNumParticles);
}

size_t ParticleSystem::AddNewParticles(ParticleSystemParticles& particles, size_t newParticles) const
{
	const size_t fromIndex = particles.array_size();
	const size_t newSize = LimitParticleCount(fromIndex + newParticles);
	particles.array_resize(newSize);
	return min(fromIndex, newSize);
}

void ParticleSystem::SimulateParticles (const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, const size_t fromIndex, float dt)
{
	size_t particleCount = ps.array_size();
	for (size_t q = fromIndex; q < particleCount; )
	{
		ps.lifetime[q] -= dt;

#if UNITY_EDITOR
		if(ParticleSystemEditor::GetIsExtraInterpolationStep())
		{
			ps.lifetime[q] = max(ps.lifetime[q], 0.0f);
		}
		else
#endif

		if (ps.lifetime[q] < 0)
		{
			KillParticle(roState, state, ps, q, particleCount);
			continue;
		}
		++q;
	}
	ps.array_resize(particleCount);

	for (size_t q = fromIndex; q < particleCount; ++q)
		ps.position[q] += (ps.velocity[q] + ps.animatedVelocity[q]) * dt;

	if(ps.usesRotationalSpeed)
		for (size_t q = fromIndex; q < particleCount; ++q)
			ps.rotation[q] += ps.rotationalSpeed[q] * dt;
}

// Copy staging particles into real particle buffer
void ParticleSystem::AddStagingBuffer(ParticleSystem& system)
{
	if(0 == system.m_ParticlesStaging.array_size())
		return;

	bool needsAxisOfRotation = system.m_Particles[kParticleBuffer0].usesAxisOfRotation;
	bool needsEmitAccumulator = system.m_Particles[kParticleBuffer0].numEmitAccumulators > 0;

	ASSERT_RUNNING_ON_MAIN_THREAD;
	const int numParticles = system.m_Particles[kParticleBuffer0].array_size();
	const int numStaging = system.m_ParticlesStaging.array_size();
	system.m_Particles->array_resize(numParticles + numStaging);
	system.m_Particles->array_merge_preallocated(system.m_ParticlesStaging, numParticles, needsAxisOfRotation, needsEmitAccumulator);
	system.m_ParticlesStaging.array_resize(0);
}

void ParticleSystem::Emit(ParticleSystem& system, const SubEmitterEmitCommand& command, ParticleSystemEmitMode emitMode)
{
	int amountOfParticlesToEmit = command.particlesToEmit;
	if (amountOfParticlesToEmit > 0)
	{
		ParticleSystemState& state = *system.m_State;
		const ParticleSystemReadOnlyState& roState = *system.m_ReadOnlyState;

		const int numContinuous = command.particlesToEmitContinuous;
		const float deltaTime = command.deltaTime;
		float parentT = command.parentT;
		Vector3f position = command.position;
		Vector3f initialVelocity = command.velocity;

		Matrix3x3f rotMat;
		Vector3f normalizedVelocity = NormalizeSafe(initialVelocity);
		float angle = Abs(Dot(normalizedVelocity, Vector3f::zAxis));
		Vector3f up = Lerp(Vector3f::zAxis, Vector3f::yAxis, angle);
		if (!LookRotationToMatrix(normalizedVelocity, up, &rotMat))
			rotMat.SetIdentity();

		Matrix4x4f parentParticleMatrix = rotMat;
		parentParticleMatrix.SetPosition(position);

		// Transform into local space of sub emitter
		Matrix4x4f concatMatrix;
		if(roState.useLocalSpace)
			MultiplyMatrices3x4(state.worldToLocal, parentParticleMatrix, concatMatrix);
		else
			concatMatrix = parentParticleMatrix;

		if(roState.useLocalSpace)
			initialVelocity = state.worldToLocal.MultiplyVector3(initialVelocity);

		DebugAssert(state.GetIsSubEmitter());
		DebugAssert(state.stopEmitting);

		const float commandAliveTime = command.timeAlive;
		const float timeStep = GetTimeStep(commandAliveTime, true);

		// @TODO: Perform culling: if max lifetime < timeAlive, then just skip emit

		// Perform sub emitter loop
		if(roState.looping)
			parentT = fmodf(parentT, roState.lengthInSec);

		ParticleSystemParticles& particles = (kParticleSystemEMDirect == emitMode) ? system.m_Particles[kParticleBuffer0] : system.m_ParticlesStaging;

		size_t fromIndex = system.AddNewParticles(particles, amountOfParticlesToEmit);
		StartModules (system, roState, state, command.emissionState, initialVelocity, concatMatrix, particles, fromIndex, deltaTime, parentT, numContinuous, 0.0f);

		// Make sure particles get updated
		if(0 == fromIndex)
			system.KeepUpdating();

		// Update incremental
		if(timeStep > 0.0001f)
		{
			float accumulatedDt = commandAliveTime;
			while (accumulatedDt >= timeStep)
			{
				accumulatedDt -= timeStep;
				UpdateModulesIncremental(system, roState, state, particles, fromIndex, timeStep);
			}
		}
	}
}


void ParticleSystem::PlaybackSubEmitterCommandBuffer(const ParticleSystem& parent, ParticleSystemState& state, bool fixedTimeStep)
{
	ParticleSystem* subEmittersBirth[kParticleSystemMaxSubBirth];
	ParticleSystem* subEmittersCollision[kParticleSystemMaxSubCollision];
	ParticleSystem* subEmittersDeath[kParticleSystemMaxSubDeath];
	int numBirth = parent.m_SubModule->GetSubEmitterPtrsBirth(&subEmittersBirth[0]);
	int numCollision = parent.m_SubModule->GetSubEmitterPtrsCollision(&subEmittersCollision[0]);
	int numDeath = parent.m_SubModule->GetSubEmitterPtrsDeath(&subEmittersDeath[0]);

	const int numCommands = state.subEmitterCommandBuffer.commandCount;
	const SubEmitterEmitCommand* commands = state.subEmitterCommandBuffer.commands;
	for(int i = 0; i < numCommands; i++)
	{
		const SubEmitterEmitCommand& command = commands[i];
		ParticleSystem* shuriken = NULL;
		if(command.subEmitterType == kParticleSystemSubTypeBirth)
			shuriken = (command.subEmitterIndex < numBirth) ? subEmittersBirth[command.subEmitterIndex] : NULL;
		else if(command.subEmitterType == kParticleSystemSubTypeCollision)
			shuriken = (command.subEmitterIndex < numCollision) ? subEmittersCollision[command.subEmitterIndex] : NULL;
		else if(command.subEmitterType == kParticleSystemSubTypeDeath)
			shuriken = (command.subEmitterIndex < numDeath) ? subEmittersDeath[command.subEmitterIndex] : NULL;
		else
			Assert(!"PlaybackSubEmitterCommandBuffer: Sub emitter type not implemented");

		DebugAssert(shuriken && "Y U NO HERE ANYMORE?");
		if(!shuriken)
			continue;

		ParticleSystem::Emit(*shuriken, command, kParticleSystemEMDirect);
	}

	state.subEmitterCommandBuffer.commandCount = 0;
}

void ParticleSystem::UpdateBounds(const ParticleSystem& system, const ParticleSystemParticles& ps, ParticleSystemState& state)
{
	const size_t particleCount = ps.array_size();
	if (particleCount == 0)
	{
		state.minMaxAABB = MinMaxAABB (Vector3f::zero, Vector3f::zero);
		return;
	}

	state.minMaxAABB.Init();

	if(CheckSupportsProcedural(system))
	{
		const Transform& transform = system.GetComponent (Transform);
		Matrix4x4f localToWorld = transform.GetLocalToWorldMatrixNoScale ();

		// Lifetime and max speed
		const Vector2f minMaxLifeTime = system.m_InitialModule.GetLifeTimeCurve().FindMinMax();

		const float maxLifeTime = minMaxLifeTime.y;
		const Vector2f minMaxStartSpeed = system.m_InitialModule.GetSpeedCurve().FindMinMax() * maxLifeTime;

		state.minMaxAABB = MinMaxAABB(Vector3f::zero, Vector3f::zero);
		state.minMaxAABB.Encapsulate(Vector3f::zAxis * minMaxStartSpeed.x);
		state.minMaxAABB.Encapsulate(Vector3f::zAxis * minMaxStartSpeed.y);
		if(system.m_ShapeModule.GetEnabled())
			system.m_ShapeModule.CalculateProceduralBounds(state.minMaxAABB, state.emitterScale, minMaxStartSpeed);

		// Gravity
		// @TODO: Do what we do for force and velocity here, i.e. transform bounds properly
		const Vector3f gravityBounds = system.m_InitialModule.GetGravity(*system.m_ReadOnlyState, *system.m_State) * maxLifeTime * maxLifeTime * 0.5f;

		state.minMaxAABB.m_Max += max(gravityBounds, Vector3f::zero);
		state.minMaxAABB.m_Min += min(gravityBounds, Vector3f::zero);

		MinMaxAABB velocityBounds (Vector3f::zero, Vector3f::zero);
		if(system.m_VelocityModule->GetEnabled())
			system.m_VelocityModule->CalculateProceduralBounds(velocityBounds, localToWorld, maxLifeTime);
		state.minMaxAABB.m_Max += velocityBounds.m_Max;
		state.minMaxAABB.m_Min += velocityBounds.m_Min;

		MinMaxAABB forceBounds (Vector3f::zero, Vector3f::zero);
		if(system.m_ForceModule->GetEnabled())
			system.m_ForceModule->CalculateProceduralBounds(forceBounds, localToWorld, maxLifeTime);
		state.minMaxAABB.m_Max += forceBounds.m_Max;
		state.minMaxAABB.m_Min += forceBounds.m_Min;

		// Modules that are not supported by procedural
		DebugAssert(!system.m_RotationBySpeedModule->GetEnabled()); // find out if possible to support it
		DebugAssert(!system.m_ClampVelocityModule->GetEnabled()); // unsupported: (Need to compute velocity by deriving position curves...), possible to support?
		DebugAssert(!system.m_CollisionModule->GetEnabled());
		DebugAssert(!system.m_SubModule->GetEnabled()); // find out if possible to support it
		DebugAssert(!system.m_ExternalForcesModule->GetEnabled());
	}
	else
	{
		for (size_t q = 0; q < particleCount; ++q)
			state.minMaxAABB.Encapsulate (ps.position[q]);

		ParticleSystemRenderer* renderer = system.QueryComponent(ParticleSystemRenderer);
		if(renderer && (renderer->GetRenderMode() == kSRMStretch3D))
		{
			const float velocityScale = renderer->GetVelocityScale();
			const float lengthScale = renderer->GetLengthScale();
			for(size_t q = 0; q < particleCount; ++q )
			{
				float sqrVelocity = SqrMagnitude (ps.velocity[q]+ps.animatedVelocity[q]);
				if (sqrVelocity > Vector3f::epsilon)
				{
					float scale = velocityScale + FastInvSqrt (sqrVelocity) * lengthScale * ps.size[q];
					state.minMaxAABB.Encapsulate(ps.position[q] - ps.velocity[q] * scale);
				}
			}
		}
	}

	// Expand with maximum particle size * sqrt(2) (the length of a diagonal of a particle, if it should happen to be rotated)
	const float kSqrtOf2 = 1.42f;
	float maxSize = kSqrtOf2 * 0.5f * system.m_InitialModule.GetSizeCurve().FindMinMax().y;
	if(system.m_SizeModule->GetEnabled())
		maxSize *= system.m_SizeModule->GetCurve().FindMinMax().y;
	if(system.m_SizeBySpeedModule->GetEnabled())
		maxSize *= system.m_SizeBySpeedModule->GetCurve().FindMinMax().y;
	state.minMaxAABB.Expand (max(maxSize, state.maxSize));

	Assert (state.minMaxAABB.IsValid ());
}

IMPLEMENT_CLASS_HAS_INIT (ParticleSystem)
IMPLEMENT_OBJECT_SERIALIZE (ParticleSystem)


template<class TransferFunction>
void ParticleSystem::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	m_ReadOnlyState->Transfer (transfer);												m_ReadOnlyState->CheckConsistency();
	m_State->Transfer (transfer);
	transfer.Transfer(m_InitialModule, m_InitialModule.GetName ());						m_InitialModule.CheckConsistency ();
	transfer.Transfer(m_ShapeModule, m_ShapeModule.GetName ());							m_ShapeModule.CheckConsistency ();
	transfer.Transfer(m_EmissionModule, m_EmissionModule.GetName ());					m_EmissionModule.CheckConsistency ();
	transfer.Transfer(*m_SizeModule, m_SizeModule->GetName ());							m_SizeModule->CheckConsistency ();
	transfer.Transfer(*m_RotationModule, m_RotationModule->GetName ());					m_RotationModule->CheckConsistency ();
	transfer.Transfer(*m_ColorModule, m_ColorModule->GetName ());						m_ColorModule->CheckConsistency ();
	transfer.Transfer(*m_UVModule, m_UVModule->GetName ());								m_UVModule->CheckConsistency ();
	transfer.Transfer(*m_VelocityModule, m_VelocityModule->GetName ());					m_VelocityModule->CheckConsistency ();
	transfer.Transfer(*m_ForceModule, m_ForceModule->GetName ());						m_ForceModule->CheckConsistency ();
	transfer.Transfer(*m_ExternalForcesModule, m_ExternalForcesModule->GetName ());		m_ExternalForcesModule->CheckConsistency ();
	transfer.Transfer(*m_ClampVelocityModule, m_ClampVelocityModule->GetName ());		m_ClampVelocityModule->CheckConsistency ();
	transfer.Transfer(*m_SizeBySpeedModule, m_SizeBySpeedModule->GetName ());			m_SizeBySpeedModule->CheckConsistency ();
	transfer.Transfer(*m_RotationBySpeedModule, m_RotationBySpeedModule->GetName ());	m_RotationBySpeedModule->CheckConsistency ();
	transfer.Transfer(*m_ColorBySpeedModule, m_ColorBySpeedModule->GetName ());			m_ColorBySpeedModule->CheckConsistency ();
	transfer.Transfer(*m_CollisionModule, m_CollisionModule->GetName ());				m_CollisionModule->CheckConsistency ();
	transfer.Transfer(*m_SubModule, m_SubModule->GetName ());							m_SubModule->CheckConsistency ();
	if(transfer.IsReading())
	{
		m_State->supportsProcedural = DetermineSupportsProcedural(*this);
		m_State->invalidateProcedural = true; // Stuff might have changed which we can't support (example: start speed has become smaller)
	}
}

bool ParticleSystem::CheckSupportsProcedural(const ParticleSystem& system)
{
	ParticleSystemRenderer* renderer = system.QueryComponent(ParticleSystemRenderer);
	if(renderer && (renderer->GetRenderMode() == kSRMStretch3D))
		return false;
	return system.m_State->supportsProcedural && !system.m_State->invalidateProcedural;
}

// TODO: Needs to match UpdateCullingSupportedString in all xxModule.cs files
bool ParticleSystem::DetermineSupportsProcedural(const ParticleSystem& system)
{
	bool supportsProcedural = true;
	supportsProcedural = supportsProcedural && system.m_ReadOnlyState->useLocalSpace;

	// Can't be random as we recreate it every frame. TODO: Would be great if we can support this in procedural mode.
	const short lifeTimeMinMaxState = system.m_InitialModule.GetLifeTimeCurve().minMaxState;
	supportsProcedural = supportsProcedural && (lifeTimeMinMaxState == kMMCScalar || lifeTimeMinMaxState == kMMCCurve);

	supportsProcedural = supportsProcedural && (EmissionModule::kEmissionTypeDistance != system.m_EmissionModule.GetEmissionDataRef().type);

	supportsProcedural = supportsProcedural && !system.m_ExternalForcesModule->GetEnabled();
	supportsProcedural = supportsProcedural && !system.m_ClampVelocityModule->GetEnabled();
	supportsProcedural = supportsProcedural && !system.m_RotationBySpeedModule->GetEnabled();
	supportsProcedural = supportsProcedural && !system.m_CollisionModule->GetEnabled();
	supportsProcedural = supportsProcedural && !system.m_SubModule->GetEnabled();

	if(system.m_RotationModule->GetEnabled())
		supportsProcedural = supportsProcedural && CurvesSupportProcedural (system.m_RotationModule->GetCurve().editorCurves, system.m_RotationModule->GetCurve().minMaxState);

	if(system.m_VelocityModule->GetEnabled())
	{
		supportsProcedural = supportsProcedural && CurvesSupportProcedural (system.m_VelocityModule->GetXCurve().editorCurves, system.m_VelocityModule->GetXCurve().minMaxState);
		supportsProcedural = supportsProcedural && CurvesSupportProcedural (system.m_VelocityModule->GetYCurve().editorCurves, system.m_VelocityModule->GetYCurve().minMaxState);
		supportsProcedural = supportsProcedural && CurvesSupportProcedural (system.m_VelocityModule->GetZCurve().editorCurves, system.m_VelocityModule->GetZCurve().minMaxState);
	}

	if(system.m_ForceModule->GetEnabled())
	{
		supportsProcedural = supportsProcedural && CurvesSupportProcedural (system.m_ForceModule->GetXCurve().editorCurves, system.m_ForceModule->GetXCurve().minMaxState);
		supportsProcedural = supportsProcedural && CurvesSupportProcedural (system.m_ForceModule->GetYCurve().editorCurves, system.m_ForceModule->GetYCurve().minMaxState);
		supportsProcedural = supportsProcedural && CurvesSupportProcedural (system.m_ForceModule->GetZCurve().editorCurves, system.m_ForceModule->GetZCurve().minMaxState);
		supportsProcedural = supportsProcedural && !system.m_ForceModule->GetRandomizePerFrame();
	}

	return supportsProcedural;
}

bool ParticleSystem::ComputePrewarmStartParameters(float& prewarmTime, float t)
{
	const float fixedDelta = GetTimeManager().GetFixedDeltaTime();
	const float maxLifetime = m_InitialModule.GetLifeTimeCurve().FindMinMax().y;
	const float length = m_ReadOnlyState->lengthInSec;
	if(!m_ReadOnlyState->looping &&  ((maxLifetime + length) < t))
		return false;

	prewarmTime = m_SubModule->GetEnabled() ? CalculateSubEmitterMaximumLifeTime(maxLifetime) : 0.0f;
	prewarmTime = max(prewarmTime, maxLifetime);

	float frac = fmodf(t, fixedDelta);
	float startT = t - prewarmTime - frac;
	prewarmTime += frac;

	// Clamp to start
	if(!m_ReadOnlyState->prewarm)
	{
		startT = max(startT, 0.0f);
		prewarmTime = min(t, prewarmTime);
	}

	// This is needed to figure out if emitacc should be inverted or not
	const float signStartT = Sign(startT);
	const float absStartT = Abs(startT);

	while(startT < 0.0f)
		startT += length;
	float endT = startT + absStartT;
	m_State->t = fmodf(startT, length);

	ParticleSystemEmissionState emissionState;
	const Vector3f emitVelocity = Vector3f::zero;
	const float epsilon = 0.0001f;
	int seedIndex = 0;

	const float prevStartT = startT;
	const float nextStartT = startT + fixedDelta;
	const float prevEndT = endT;
	const float nextEndT = endT + fixedDelta;
	if (!(nextStartT > prevStartT && nextEndT > prevEndT))
	{
		ErrorStringObject ("Precision issue while prewarming particle system - 'Duration' or 'Start Lifetime' is likely a too large value.", this);
		return false;
	}

	while((startT + epsilon) < endT)
	{
		const float toT = fmodf(startT + fixedDelta, length);
		const float fromT = fmodf(startT, length);

		size_t numContinuous;
		int numParticles = ParticleSystem::EmitFromModules(*this, *m_ReadOnlyState, emissionState, numContinuous, emitVelocity, fromT, toT, fixedDelta);
		if(numParticles > 0)
			seedIndex++;
		startT += fixedDelta;
	}
	m_State->emissionState.m_ToEmitAccumulator = ((signStartT > 0.0f ? emissionState.m_ToEmitAccumulator : 1.0f - emissionState.m_ToEmitAccumulator)) + epsilon;
#if UNITY_EDITOR
	m_EditorRandomSeedIndex = signStartT > 0.0f ? seedIndex : -seedIndex-1;
#endif

	return true;
}

void ParticleSystem::Simulate (float t, bool restart)
{
	PROFILER_AUTO(gParticleSystemPrewarm, NULL)

	if(restart)
	{
		m_InitialModule.ResetSeed(*m_ReadOnlyState);
		m_ShapeModule.ResetSeed(*m_ReadOnlyState);
		Stop();
		Clear();
		Play (false);

		ApplyStartDelay (m_State->delayT, t);
		float prewarmTime;
		if(ComputePrewarmStartParameters(prewarmTime, t))
		{
			Update(*this, prewarmTime, true, CheckSupportsProcedural(*this));
			Pause ();
		}
		else
		{
			Stop();
			Clear();
		}
	}
	else
	{
		m_State->playing = true;
		Update(*this, t, true, false);
		Pause ();
	}
}

void ParticleSystem::Play (bool autoPrewarm)
{
	Assert (m_State);
	if(!IsActive () || m_State->GetIsSubEmitter())
		return;

	m_State->stopEmitting = false;
	m_State->playing = true;
	if (m_State->needRestart)
	{
		if(m_ReadOnlyState->prewarm)
		{
			if (autoPrewarm)
				AutoPrewarm();
		}
		else
		{
			m_State->delayT = m_ReadOnlyState->startDelay;
		}

		m_State->playing = true;
		m_State->t = 0.0f;
		m_State->numLoops = 0;
		m_State->invalidateProcedural = false;
		m_State->accumulatedDt = 0.0f;
#if UNITY_EDITOR
		m_EditorRandomSeedIndex = 0;
#endif
		m_State->emissionState.Clear();
	}

	if(m_State->culled && CheckSupportsProcedural(*this))
		Cull();
	else
		AddToManager();
}

void ParticleSystem::AutoPrewarm()
{
	if(m_ReadOnlyState->prewarm && m_ReadOnlyState->looping)
	{
		DebugAssert(!m_State->GetIsSubEmitter());
		DebugAssert(m_State->playing); // Only use together with play
		Simulate (0.0f, true);
		//DebugAssert(CompareApproximately(m_State->emissionState.m_ToEmitAccumulator, 1.0f, 0.01f) && "Particle emission gaps may occur");
	}
}

void ParticleSystem::Stop ()
{
	Assert (m_State);
	m_State->needRestart = true;
	m_State->stopEmitting = true;
}

void ParticleSystem::Pause ()
{
	Assert (m_State);
	m_State->playing = false;
	m_State->needRestart = false;
	RemoveFromManager();
}

bool ParticleSystem::IsPaused () const
{
	Assert (m_State);
	return !m_State->playing && !m_State->needRestart;
}

bool ParticleSystem::IsStopped () const
{
	Assert (m_State);
	return !m_State->playing && m_State->needRestart;
}

void ParticleSystem::KeepUpdating()
{
	if (IsActive())
 	{
 		// Ensure added particles will update, but stop emission
 		m_State->playing = true;
 		m_State->stopEmitting = true;
		AddToManager();
 	}
}

void ParticleSystem::Emit (int count)
{
	// StartParticles() takes size_t so check for negative value here (case 495098)
	if (count <= 0)
		return;

	KeepUpdating();
	Assert (m_State);

	const Transform& transform = GetComponent (Transform);
	Matrix4x4f oldLocalToWorld = m_State->localToWorld;
	Matrix4x4f oldWorldToLocal = m_State->worldToLocal;
	Vector3f oldEmitterScale = m_State->emitterScale;
	m_State->localToWorld = transform.GetLocalToWorldMatrixNoScale ();
	Matrix4x4f::Invert_General3D(m_State->localToWorld, m_State->worldToLocal);
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_2_a1))
		m_State->emitterScale = transform.GetWorldScaleLossy();
	else
		m_State->emitterScale = transform.GetLocalScale();
	StartParticles(*this, m_Particles[kParticleBuffer0], 0.0f, m_State->t, 0.0f, 0, count, 0.0f);
	m_State->localToWorld = oldLocalToWorld;
	m_State->worldToLocal = oldWorldToLocal;
	m_State->emitterScale = oldEmitterScale;
}

void ParticleSystem::EmitParticleExternal(ParticleSystemParticle* particle)
{
#if UNITY_EDITOR
	if(!IsWorldPlaying() && IsStopped ())
		return;
#endif

	m_State->invalidateProcedural = true;

	CheckParticleConsistency(*m_State, *particle);
	if(particle->lifetime <= 0.0f)
		return;

	KeepUpdating();

	m_Particles[kParticleBuffer0].AddParticle(particle);

#if UNITY_EDITOR
	if(!IsWorldPlaying())
		m_Particles[kParticleBuffer1].AddParticle(particle);
#endif

	if(!IsPlaying())
		UpdateBounds(*this, m_Particles[kParticleBuffer0], *m_State);
}

void ParticleSystem::Clear (bool updateBounds)
{
	for(int i = 0; i < kNumParticleBuffers; i++)
		m_Particles[i].array_resize(0);
	m_ParticlesStaging.array_resize(0);
	m_State->emitReplay.resize_uninitialized(0);

	if (m_State->stopEmitting)
	{
		// This triggers sometimes, why? (case 491684)
		DebugAssert (m_State->needRestart);
		m_State->playing = false;
		RemoveFromManager();
	}

	if(updateBounds)
	{
		UpdateBounds(*this, m_Particles[kParticleBuffer0], *m_State);
		Update2(*this, *m_ReadOnlyState, *m_State, false);
	}
}

float ParticleSystem::GetStartDelay() const
{
	Assert(m_State);
	return m_ReadOnlyState->startDelay;
}

void ParticleSystem::SetStartDelay(float value)
{
	Assert(m_State);
	m_ReadOnlyState->startDelay = value;
	SetDirty ();
}

bool ParticleSystem::IsAlive () const
{
	Assert(m_State);
	return (!IsStopped() || (GetParticleCount() > 0));
}

bool ParticleSystem::IsPlaying () const
{
	Assert (m_State);
	return m_State->playing;
}

bool ParticleSystem::GetLoop () const
{
	Assert (m_State);
	return m_ReadOnlyState->looping;
}

void ParticleSystem::SetLoop (bool loop)
{
	Assert (m_State);
	m_ReadOnlyState->looping = loop;
	SetDirty ();
}

bool ParticleSystem::GetPlayOnAwake () const
{
	Assert (m_State);
	return m_ReadOnlyState->playOnAwake;
}

void ParticleSystem::SetPlayOnAwake (bool playOnAwake)
{
	Assert (m_State);
	m_ReadOnlyState->playOnAwake = playOnAwake;
	SetDirty ();
}

ParticleSystemSimulationSpace ParticleSystem::GetSimulationSpace () const
{
	Assert (m_State);
	return (m_ReadOnlyState->useLocalSpace ? kSimLocal : kSimWorld);
}

void ParticleSystem::SetSimulationSpace (ParticleSystemSimulationSpace simulationSpace)
{
	Assert (m_State);
	m_ReadOnlyState->useLocalSpace = (simulationSpace == kSimLocal);
	SetDirty ();
}

float ParticleSystem::GetSecPosition () const
{
	Assert (m_State);
	return m_State->t;
}

void ParticleSystem::SetSecPosition (float pos)
{
	Assert (m_State);
	m_State->t = pos;
	SetDirty ();
}

float ParticleSystem::GetPlaybackSpeed () const
{
	Assert (m_State);
	return m_ReadOnlyState->speed;
}

void ParticleSystem::SetPlaybackSpeed (float speed)
{
	Assert (m_State);
	m_ReadOnlyState->speed = speed;
	SetDirty ();
}

float ParticleSystem::GetLengthInSec () const
{
	Assert (m_State);
	return m_ReadOnlyState->lengthInSec;
}

void ParticleSystem::GetNumTiles(int& uvTilesX, int& uvTilesY) const
{
	uvTilesX = uvTilesY = 1;
	if(m_UVModule->GetEnabled())
		m_UVModule->GetNumTiles(uvTilesX, uvTilesY);
}

Matrix4x4f ParticleSystem::GetLocalToWorldMatrix() const
{
	return m_State->localToWorld;
}

bool ParticleSystem::GetEnableEmission() const
{
	return m_EmissionModule.GetEnabled();
}

void ParticleSystem::SetEnableEmission(bool value)
{
	m_State->invalidateProcedural = true;
	m_EmissionModule.SetEnabled(value);
	SetDirty();
}

float ParticleSystem::GetEmissionRate() const
{
	return m_EmissionModule.GetEmissionDataRef().rate.GetScalar();
}

void ParticleSystem::SetEmissionRate(float value)
{
	m_State->invalidateProcedural = true;
	m_EmissionModule.GetEmissionData().rate.SetScalar(value);
	SetDirty();
}

float ParticleSystem::GetStartSpeed() const
{
	return m_InitialModule.GetSpeedCurve().GetScalar();
}

void ParticleSystem::SetStartSpeed(float value)
{
	m_State->invalidateProcedural = true;
	m_InitialModule.GetSpeedCurve().SetScalar(value);
	SetDirty();
}

float ParticleSystem::GetStartSize() const
{
	return m_InitialModule.GetSizeCurve().GetScalar();
}

void ParticleSystem::SetStartSize(float value)
{
	m_InitialModule.GetSizeCurve().SetScalar(value);
	SetDirty();
}

ColorRGBAf ParticleSystem::GetStartColor() const
{
	return m_InitialModule.GetColor().maxColor;
}

void ParticleSystem::SetStartColor(ColorRGBAf value)
{
	m_InitialModule.GetColor().maxColor = value;
	SetDirty();
}

float ParticleSystem::GetStartRotation() const
{
	return m_InitialModule.GetRotationCurve().GetScalar();
}

void ParticleSystem::SetStartRotation(float value)
{
	m_InitialModule.GetRotationCurve().SetScalar(value);
	SetDirty();
}

float ParticleSystem::GetStartLifeTime() const
{
	return m_InitialModule.GetLifeTimeCurve().GetScalar();
}

void ParticleSystem::SetStartLifeTime(float value)
{
	m_State->invalidateProcedural = true;
	m_InitialModule.GetLifeTimeCurve().SetScalar(value);
	SetDirty();
}

float ParticleSystem::GetGravityModifier() const
{
	return m_InitialModule.GetGravityModifier();
}

void ParticleSystem::SetGravityModifier(float value)
{
	m_State->invalidateProcedural = true;
	m_InitialModule.SetGravityModifier(value);
	SetDirty();
}

UInt32 ParticleSystem::GetRandomSeed() const
{
	return m_ReadOnlyState->randomSeed;
}

void ParticleSystem::SetRandomSeed(UInt32 value)
{
	m_ReadOnlyState->randomSeed = value;
	SetDirty();
}

int ParticleSystem::GetMaxNumParticles () const
{
	return m_InitialModule.GetMaxNumParticles ();
}

void ParticleSystem::SetMaxNumParticles (int value)
{
	m_InitialModule.SetMaxNumParticles (value);
	SetDirty();
}

void ParticleSystem::AllocateAllStructuresOfArrays()
{
	// Make sure all particle buffers have all elements
	for(int i = 0; i < kNumParticleBuffers; i++)
	{
		if(!m_Particles[i].usesAxisOfRotation)
			m_Particles[i].SetUsesAxisOfRotation();
		m_Particles[i].SetUsesEmitAccumulator(kParticleSystemMaxNumEmitAccumulators);
	}
}

void ParticleSystem::SetParticlesExternal (ParticleSystemParticle* particles, int size)
{
#if UNITY_EDITOR
	if(!IsWorldPlaying() && IsStopped ())
		return;
#endif

	m_State->invalidateProcedural = true;

	// Make sure particles are in the correct ranges etc.
	for (size_t q = 0; q < size; q++)
		CheckParticleConsistency(*m_State, particles[q]);

	AllocateAllStructuresOfArrays();
	ParticleSystemParticles& ps = m_Particles[kParticleBuffer0];
	ps.array_resize(size);
	ps.CopyFromArrayAOS(particles, size);
	size_t particleCount = size;
	for (size_t q = 0; q < particleCount; )
	{
			if (ps.lifetime[q] < 0)
			{
				KillParticle(*m_ReadOnlyState, *m_State, ps, q, particleCount);
				continue;
			}
			++q;
	}
	ps.array_resize(particleCount);

	#if UNITY_EDITOR
	if(!IsWorldPlaying())
		m_Particles[kParticleBuffer1].array_assign(ps);
	#endif

	if(!IsPlaying())
		UpdateBounds(*this, ps, *m_State);
}

void ParticleSystem::GetParticlesExternal (ParticleSystemParticle* particles, int size)
{
	AllocateAllStructuresOfArrays();
	ParticleSystemParticles& ps = m_Particles[kParticleBuffer0];
	ps.CopyToArrayAOS(particles, size);
}

int ParticleSystem::GetSafeCollisionEventSize () const
{
	const ParticleSystemParticles& ps = m_Particles[kParticleBuffer0];
	return ps.collisionEvents.GetCollisionEventCount ();
}

int ParticleSystem::GetCollisionEventsExternal (int instanceID, MonoParticleCollisionEvent* collisionEvents, int size) const
{
	const ParticleSystemParticles& ps = m_Particles[kParticleBuffer0];
	return ps.collisionEvents.GetCollisionEvents (instanceID, collisionEvents, size);
}

ParticleSystemParticles& ParticleSystem::GetParticles (int index)
{
#if UNITY_EDITOR
	if(index == -1)
		if(ParticleSystemEditor::UseInterpolation(*this))
			index = (int)kParticleBuffer1;
		else
			index = (int)kParticleBuffer0;
	return m_Particles[index];
#else
	return m_Particles[kParticleBuffer0];
#endif
}

const ParticleSystemParticles& ParticleSystem::GetParticles (int index) const
{
#if UNITY_EDITOR
	if(index == -1)
		if(ParticleSystemEditor::UseInterpolation(*this))
			index = (int)kParticleBuffer1;
		else
			index = (int)kParticleBuffer0;
	return m_Particles[index];
#else
	return m_Particles[kParticleBuffer0];
#endif
}

size_t ParticleSystem::GetParticleCount () const
{
	return m_Particles[kParticleBuffer0].array_size ();
}

int ParticleSystem::SetupSubEmitters(ParticleSystem& shuriken, ParticleSystemState& state)
{
	Assert(!state.cachedSubDataBirth && !state.numCachedSubDataBirth);
	Assert(!state.cachedSubDataCollision && !state.numCachedSubDataCollision);
	Assert(!state.cachedSubDataDeath && !state.numCachedSubDataDeath);
	int subEmitterCount = 0;

	if(shuriken.m_SubModule->GetEnabled())
	{
		ParticleSystem* subEmittersBirth[kParticleSystemMaxSubBirth];
		state.numCachedSubDataBirth = shuriken.m_SubModule->GetSubEmitterPtrsBirth(&subEmittersBirth[0]);
		state.cachedSubDataBirth = ALLOC_TEMP_MANUAL(ParticleSystemSubEmitterData, state.numCachedSubDataBirth);
		std::uninitialized_fill (state.cachedSubDataBirth, state.cachedSubDataBirth + state.numCachedSubDataBirth, ParticleSystemSubEmitterData());
		for(int i = 0; i < state.numCachedSubDataBirth; i++)
		{
			ParticleSystem* subEmitter = subEmittersBirth[i];
			ParticleSystemSubEmitterData& subData = state.cachedSubDataBirth[i];
			subData.startDelayInSec = subEmitter->m_ReadOnlyState->startDelay;
			subData.lengthInSec = subEmitter->GetLoop() ? numeric_limits<float>::max() : subEmitter->GetLengthInSec();
			subData.maxLifetime = subEmitter->m_InitialModule.GetLifeTimeCurve().GetScalar();
			subData.emitter = subEmitter;
			subEmitter->m_EmissionModule.GetEmissionDataCopy(&subData.emissionData);
			subEmitter->m_State->SetIsSubEmitter(true);
			subEmitterCount++;
		}
		ParticleSystem* subEmittersCollision[kParticleSystemMaxSubCollision];
		state.numCachedSubDataCollision = shuriken.m_SubModule->GetSubEmitterPtrsCollision(&subEmittersCollision[0]);
		state.cachedSubDataCollision = ALLOC_TEMP_MANUAL(ParticleSystemSubEmitterData, state.numCachedSubDataCollision);
		std::uninitialized_fill (state.cachedSubDataCollision, state.cachedSubDataCollision + state.numCachedSubDataCollision, ParticleSystemSubEmitterData());
		for(int i = 0; i < state.numCachedSubDataCollision; i++)
		{
			ParticleSystem* subEmitter = subEmittersCollision[i];
			ParticleSystemSubEmitterData& subData = state.cachedSubDataCollision[i];
			subData.emitter = subEmitter;
			subEmitter->m_EmissionModule.GetEmissionDataCopy(&subData.emissionData);
			subEmitter->m_State->SetIsSubEmitter(true);
			subEmitterCount++;
		}
		ParticleSystem* subEmittersDeath[kParticleSystemMaxSubDeath];
		state.numCachedSubDataDeath = shuriken.m_SubModule->GetSubEmitterPtrsDeath(&subEmittersDeath[0]);
		state.cachedSubDataDeath = ALLOC_TEMP_MANUAL(ParticleSystemSubEmitterData, state.numCachedSubDataDeath);
		std::uninitialized_fill (state.cachedSubDataDeath, state.cachedSubDataDeath + state.numCachedSubDataDeath, ParticleSystemSubEmitterData());
		for(int i = 0; i < state.numCachedSubDataDeath; i++)
		{
			ParticleSystem* subEmitter = subEmittersDeath[i];
			ParticleSystemSubEmitterData& subData = state.cachedSubDataDeath[i];
			subData.emitter = subEmitter;
			subEmitter->m_EmissionModule.GetEmissionDataCopy(&subData.emissionData);
			subEmitter->m_State->SetIsSubEmitter(true);
			subEmitterCount++;
		}
	}
	return subEmitterCount;
}

int ParticleSystem::CalculateMaximumSubEmitterEmitCount(ParticleSystem& shuriken, ParticleSystemState& state, float deltaTime, bool fixedTimeStep)
{
	// Save emission state
	const ParticleSystemEmissionState orgEmissionState = state.emissionState;
	ParticleSystemEmissionState emissionState;

	// Simulate emission
	float timeStep = GetTimeStep(deltaTime, fixedTimeStep);
	DebugAssert(timeStep > 0.0001f);

	// @TODO: Maybe we can go back to just picking the conservative solution, since no longer use this for scrubbing/prewarm we shouldn't allocate that much.


	int existingParticleCount = shuriken.GetParticleCount();
	int totalPossibleEmits = 0;
	float acc = deltaTime;
	float fromT = state.t;
	while(acc >= timeStep)
	{
		acc -= timeStep;
		float toT = fromT + (deltaTime - acc);
		float dt;
		const float length = shuriken.m_ReadOnlyState->lengthInSec;
		if(shuriken.m_ReadOnlyState->looping)
		{
			toT = fmodf (toT, length);
			dt = timeStep;
		}
		else
		{
			toT = min(toT, length);
			dt = toT - fromT;
		}

		size_t numContinuous;
		const Vector3f emitVelocity = state.emitterVelocity;
		existingParticleCount += EmitFromModules(shuriken, *shuriken.m_ReadOnlyState, emissionState, numContinuous, emitVelocity, fromT, toT, dt);
		totalPossibleEmits += existingParticleCount;
		fromT = toT;
	}

	totalPossibleEmits += existingParticleCount;

	// Restore emission state
	state.emissionState = orgEmissionState;

	const int kExtra = 5; 	// Give a few extra to avoid denying due to rounding etc.
	return totalPossibleEmits + kExtra;
}

// @TODO: what about chained effects?
float ParticleSystem::CalculateSubEmitterMaximumLifeTime(float parentLifeTime) const
{
	ParticleSystem* subEmitters[kParticleSystemMaxSubTotal];
	m_SubModule->GetSubEmitterPtrs(subEmitters);

	float maxLifeTime = 0.0f;
	for(int i = 0; i < kParticleSystemMaxSubTotal; i++)
	{
		if (subEmitters[i] && (subEmitters[i] != this))
		{
			const float emitterMaximumLifetime = subEmitters[i]->m_InitialModule.GetLifeTimeCurve().FindMinMax().y;
			maxLifeTime = max(maxLifeTime, parentLifeTime + emitterMaximumLifetime);
			maxLifeTime = std::max(maxLifeTime, subEmitters[i]->CalculateSubEmitterMaximumLifeTime(parentLifeTime + emitterMaximumLifetime));
		}
	}
	return maxLifeTime;
}

void ParticleSystem::SmartReset ()
{
	Super::SmartReset();
	AddParticleSystemRenderer ();
}

void ParticleSystem::AddParticleSystemRenderer ()
{
	if (GameObject* go = GetGameObjectPtr ())
	{
		ParticleSystemRenderer* renderer = go->QueryComponent (ParticleSystemRenderer);
		if (renderer == NULL)
		{
			string error;
			AddComponent (*go, ClassID(ParticleSystemRenderer), NULL, &error);
			if (error.empty ())
				go->GetComponent (ParticleSystemRenderer).SetMaterial (GetDefaultParticleMaterial(), 0);
			else
				LogString (Format("%s", error.c_str()));
		}
	}
}


void ParticleSystem::SetUsesRotationalSpeed()
{
	ParticleSystemParticles& ps0 = m_Particles[kParticleBuffer0];
	if(!ps0.usesRotationalSpeed)
		ps0.SetUsesRotationalSpeed ();
#if UNITY_EDITOR
	ParticleSystemParticles& ps1 = m_Particles[kParticleBuffer1];
	if(!ps1.usesRotationalSpeed)
		ps1.SetUsesRotationalSpeed ();
#endif
	ParticleSystemParticles& pss = m_ParticlesStaging;
	if(!pss.usesRotationalSpeed)
		pss.SetUsesRotationalSpeed ();
}

void ParticleSystem::SetUsesAxisOfRotation()
{
	ParticleSystemParticles& ps0 = m_Particles[kParticleBuffer0];
	if(!ps0.usesAxisOfRotation)
		ps0.SetUsesAxisOfRotation ();
#if UNITY_EDITOR
	ParticleSystemParticles& ps1 = m_Particles[kParticleBuffer1];
	if(!ps1.usesAxisOfRotation)
		ps1.SetUsesAxisOfRotation ();
#endif
	ParticleSystemParticles& pss = m_ParticlesStaging;
	if(!pss.usesAxisOfRotation)
		pss.SetUsesAxisOfRotation ();
}

void ParticleSystem::SetUsesEmitAccumulator(int numAccumulators)
{
	m_Particles[kParticleBuffer0].SetUsesEmitAccumulator (numAccumulators);
#if UNITY_EDITOR
	m_Particles[kParticleBuffer1].SetUsesEmitAccumulator (numAccumulators);
#endif
	m_ParticlesStaging.SetUsesEmitAccumulator (numAccumulators);
}

bool ParticleSystem::GetIsDistanceEmitter() const
{
	return (EmissionModule::kEmissionTypeDistance == m_EmissionModule.GetEmissionDataRef().type);
}

// check if the system would like to use any raycasting in this frame
bool ParticleSystem::SystemWannaRayCast(const ParticleSystem& system)
{
	return system.IsActive() && system.m_CollisionModule && system.m_CollisionModule->GetEnabled() && system.m_CollisionModule->IsWorldCollision() && system.m_RayBudgetState.ReceiveRays();
}

// check if the system will actually use any raycasting in this frame
bool ParticleSystem::SystemWillRayCast(const ParticleSystem& system)
{
	return system.IsActive() && system.m_CollisionModule && system.m_CollisionModule->GetEnabled() && system.m_CollisionModule->IsWorldCollision() && (system.GetRayBudget() > 0);
}

// dole out ray budgets to each system that will do raycasting
void ParticleSystem::AssignRayBudgets()
{
	int activeCount = gParticleSystemManager.activeEmitters.size();

	// count the jobs and update quality setting
	int numApproximateWorldCollisionJobs = 0;
	for(int i = 0; i < activeCount; i++)
	{
		ParticleSystem& system = *gParticleSystemManager.activeEmitters[i];
		system.m_RayBudgetState.SetQuality( system.m_CollisionModule->GetQuality() );
		system.SetRayBudget( 0 );
		if ( SystemWannaRayCast( system ) )
		{
			if ( system.m_CollisionModule->IsApproximate() )
			{
				numApproximateWorldCollisionJobs++;
			}
			else 
			{
				// high quality always get to trace all rays!
				system.SetRayBudget( (int)system.GetParticleCount() );
			}
		}
	}
	if (numApproximateWorldCollisionJobs<1) return;

	// assign ray budget to particle systems
	int totalBudget = GetQualitySettings().GetCurrent().particleRaycastBudget;
	int raysPerSystem = std::max( 0, totalBudget / numApproximateWorldCollisionJobs );
	for(int i = 0; i < activeCount; i++)
	{
		ParticleSystem& system = *gParticleSystemManager.activeEmitters[i];
		if ( SystemWannaRayCast( system ) && system.m_CollisionModule->IsApproximate() )
		{
			const int rays = std::min((int)system.GetParticleCount(),raysPerSystem);
			system.SetRayBudget( rays );
			totalBudget = std::max( totalBudget-rays, 0);
		}
	}

	// assign any remaining rays
	// TODO: possibly better to sort and go through the list incrementally updating the per system budget, than doing this hacky two pass thing
	for(int i = 0; i < activeCount; i++)
	{
		ParticleSystem& system = *gParticleSystemManager.activeEmitters[i];
		if ( SystemWannaRayCast( system ) && system.m_CollisionModule->IsApproximate() )
		{
			const int rays = std::min(totalBudget,(int)system.GetParticleCount()-system.GetRayBudget());
			system.SetRayBudget( system.GetRayBudget()+rays );
			totalBudget -= rays;
		}
		system.m_RayBudgetState.Update();
	}
}

void ParticleSystem::BeginUpdateAll ()
{
	const float deltaTimeEpsilon = 0.0001f;
	float deltaTime = GetDeltaTime();
	if(deltaTime < deltaTimeEpsilon)
		return;

	PROFILER_AUTO(gParticleSystemProfile, NULL)

	for(int i = 0; i < gParticleSystemManager.activeEmitters.size(); i++)
	{
		ParticleSystem& system = *gParticleSystemManager.activeEmitters[i];

		if (!system.IsActive ())
		{
			AssertStringObject( "UpdateParticle system should not happen on disabled vGO", &system);
			system.RemoveFromManager();
			continue;
		}

#if ENABLE_MULTITHREADED_PARTICLES
		system.m_State->recordSubEmits = true;
#else
		system.m_State->recordSubEmits = false;
#endif
		Update0 (system, *system.m_ReadOnlyState, *system.m_State, deltaTime, false);
	}

	gParticleSystemManager.needSync = true;

	// make sure ray budgets are assigned for the frame
	ParticleSystem::AssignRayBudgets();

#if ENABLE_MULTITHREADED_PARTICLES
	JobScheduler& scheduler = GetJobScheduler();
	int activeCount = gParticleSystemManager.activeEmitters.size();

	// count the jobs
	int numActiveWorldCollisionJobs = 0;
	for(int i = 0; i < activeCount; i++)
	{
		ParticleSystem& system = *gParticleSystemManager.activeEmitters[i];
		if ( system.GetRayBudget() > 0 )
			numActiveWorldCollisionJobs++;
	}

	// add collision jobs
	gParticleSystemManager.worldCollisionJobGroup = scheduler.BeginGroup(numActiveWorldCollisionJobs);
	gParticleSystemManager.jobGroup = scheduler.BeginGroup(activeCount-numActiveWorldCollisionJobs);
	for(int i = 0; i < activeCount; i++)
	{
		ParticleSystem& system = *gParticleSystemManager.activeEmitters[i];
		system.GetThreadScratchPad().deltaTime = deltaTime;
		if ( system.GetRayBudget() > 0 )
			scheduler.SubmitJob (gParticleSystemManager.worldCollisionJobGroup, ParticleSystem::UpdateFunction, &system, NULL);
		else
			scheduler.SubmitJob (gParticleSystemManager.jobGroup, ParticleSystem::UpdateFunction, &system, NULL);
	}
	scheduler.WaitForGroup(gParticleSystemManager.worldCollisionJobGroup);
#else
	for(int i = 0; i < gParticleSystemManager.activeEmitters.size(); i++)
	{
		//printf_console("BeginUpdateAll [%d]:\n",i);
		ParticleSystem& system = *gParticleSystemManager.activeEmitters[i];
		system.Update1 (system, system.GetParticles((int)ParticleSystem::kParticleBuffer0), deltaTime, false, false);
	}
#endif

}

void ParticleSystem::EndUpdateAll ()
{
	SyncJobs();

	// messages
	for (int i = 0; i < gParticleSystemManager.activeEmitters.size(); ++i)
	{
		ParticleSystem& system = *gParticleSystemManager.activeEmitters[i];
		if (!system.IsActive ())
			continue;		
		if (!system.m_CollisionModule->GetUsesCollisionMessages ())
			continue;
		ParticleSystemParticles& ps = system.GetParticles((int)ParticleSystem::kParticleBuffer0);
		ps.collisionEvents.SwapCollisionEventArrays ();
		ps.collisionEvents.SendCollisionEvents (system);
	}

	// Remove emitters that are finished (no longer emitting)
	for(int i = 0; i < gParticleSystemManager.activeEmitters.size();)
	{
		ParticleSystem& system = *gParticleSystemManager.activeEmitters[i];
		ParticleSystemState& state = *system.m_State;
		const size_t particleCount = system.GetParticleCount();
		if ((particleCount == 0) && state.playing && state.stopEmitting)
		{
			// collision subemitters may not have needRestart==true when being restarted
			// from a paused state
			//Assert (state.needRestart);
			state.playing = false;
			system.RemoveFromManager();
			continue;
		}

		i++;
	}

	// Interpolate in the editor for very low preview speeds
#if UNITY_EDITOR
	for(int i = 0; i < gParticleSystemManager.activeEmitters.size(); i++)
		ParticleSystemEditor::PerformInterpolationStep(gParticleSystemManager.activeEmitters[i]);
#endif
}

void ParticleSystem::StartParticles(ParticleSystem& system, ParticleSystemParticles& ps, const float prevT, const float t, const float dt, const size_t numContinuous, size_t amountOfParticlesToEmit, float frameOffset)
{
	if (amountOfParticlesToEmit <= 0)
		return;

	const ParticleSystemReadOnlyState& roState = *system.m_ReadOnlyState;
	ParticleSystemState& state = *system.m_State;
	size_t fromIndex = system.AddNewParticles(ps, amountOfParticlesToEmit);
	const Matrix4x4f localToWorld = !roState.useLocalSpace ? state.localToWorld : Matrix4x4f::identity;
	StartModules (system, roState, state, state.emissionState, state.emitterVelocity, localToWorld, ps, fromIndex, dt, t, numContinuous, frameOffset);
}

void ParticleSystem::StartParticlesProcedural(ParticleSystem& system, ParticleSystemParticles& ps, const float prevT, const float t, const float dt, const size_t numContinuous, size_t amountOfParticlesToEmit, float frameOffset)
{
	DebugAssert(CheckSupportsProcedural(system));

	ParticleSystemState& state = *system.m_State;

	int numParticlesRecorded = 0;
	for (int i=0;i<state.emitReplay.size();i++)
		numParticlesRecorded += state.emitReplay[i].particlesToEmit;

	float emissionOffset = state.emissionState.m_ToEmitAccumulator;
	float emissionGap = state.emissionState.m_ParticleSpacing * dt;
	amountOfParticlesToEmit = system.LimitParticleCount(numParticlesRecorded + amountOfParticlesToEmit) - numParticlesRecorded;

	if (amountOfParticlesToEmit > 0)
	{
		UInt32 randomSeed = 0;
#if UNITY_EDITOR
		ParticleSystemEditor::UpdateRandomSeed(system);
		randomSeed = ParticleSystemEditor::GetRandomSeed(system);
#endif
		state.emitReplay.push_back(ParticleSystemEmitReplay(t, amountOfParticlesToEmit, emissionOffset, emissionGap, numContinuous, randomSeed));
	}
}

void ParticleSystem::StartModules (ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, const ParticleSystemEmissionState& emissionState, Vector3f initialVelocity, const Matrix4x4f& matrix, ParticleSystemParticles& ps, size_t fromIndex, float dt, float t, size_t numContinuous, float frameOffset)
{
#if UNITY_EDITOR
	ParticleSystemEditor::UpdateRandomSeed(system);
	ParticleSystemEditor::ApplyRandomSeed(system, ParticleSystemEditor::GetRandomSeed(system));
#endif

	system.m_InitialModule.Start (roState, state, ps, matrix, fromIndex, t);
	if(system.m_ShapeModule.GetEnabled())
		system.m_ShapeModule.Start (roState, state, ps, matrix, fromIndex, t);

	DebugAssert(roState.lengthInSec > 0.0001f);
	const float normalizedT = t / roState.lengthInSec;
	DebugAssert (normalizedT >= 0.0f);
	DebugAssert (normalizedT <= 1.0f);

	size_t count = ps.array_size();
	const Vector3f velocityOffset = system.m_InitialModule.GetInheritVelocity() * initialVelocity;
	for(size_t q = fromIndex; q < count; q++)
	{
		const float randomValue = GenerateRandom(ps.randomSeed[q] + kParticleSystemStartSpeedCurveId);
		ps.velocity[q] *= Evaluate (system.m_InitialModule.GetSpeedCurve(), normalizedT, randomValue);
		ps.velocity[q] += velocityOffset;
	}

	for(size_t q = fromIndex; q < count; ) // array size changes
	{
		// subFrameOffset allows particles to be spawned at increasing times, thus spacing particles within a single frame.
		// For example if you spawn particles with high velocity you will get a continous streaming instead of a clump of particles.
		const int particleIndex = q - fromIndex;
		float subFrameOffset = (particleIndex < numContinuous) ? (float(particleIndex) + emissionState.m_ToEmitAccumulator) * emissionState.m_ParticleSpacing : 0.0f;
		DebugAssert(subFrameOffset >= -0.01f);
		DebugAssert(subFrameOffset <= 1.5f); // Not 1 due to possibly really bad precision
		subFrameOffset = clamp01(subFrameOffset);

		// Update from curves and apply forces etc.
		UpdateModulesPreSimulationIncremental (system, roState, state, ps, q, q+1, subFrameOffset * dt);

		// Position change due to where the emitter was at time of emission
		ps.position[q] -= initialVelocity * (frameOffset + subFrameOffset) * dt;

		// Position, rotation and energy change due to how much the particle has travelled since time of emission
		// @TODO: Call Simulate instead?
		ps.lifetime[q] -= subFrameOffset * dt;
		if((ps.lifetime[q] < 0.0f) && (count > 0))
		{
			KillParticle(roState, state, ps, q, count);
			continue;
		}

		ps.position[q] += (ps.velocity[q] + ps.animatedVelocity[q]) * subFrameOffset * dt;

		if(ps.usesRotationalSpeed)
			ps.rotation[q] += ps.rotationalSpeed[q] * subFrameOffset * dt;

		if(system.m_SubModule->GetEnabled())
			system.m_SubModule->Update (roState, state, ps, q, q+1, subFrameOffset * dt);

		++q;
	}
	ps.array_resize(count);
}

void ParticleSystem::UpdateModulesPreSimulationIncremental (const ParticleSystem& system, const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& particles, const size_t fromIndex, const size_t toIndex, float dt)
{
	const size_t count = particles.array_size();
	system.m_InitialModule.Update (roState, state, particles, fromIndex, toIndex, dt);
	if(system.m_RotationModule->GetEnabled())
		system.m_RotationModule->Update (roState, state, particles, fromIndex, toIndex);
	if(system.m_VelocityModule->GetEnabled())
		system.m_VelocityModule->Update (roState, state, particles, fromIndex, toIndex);
	if(system.m_ForceModule->GetEnabled())
		system.m_ForceModule->Update (roState, state, particles, fromIndex, toIndex, dt);
	if(system.m_ExternalForcesModule->GetEnabled())
		system.m_ExternalForcesModule->Update (roState, state, particles, fromIndex, toIndex, dt);
	if(system.m_ClampVelocityModule->GetEnabled())
		system.m_ClampVelocityModule->Update (roState, state, particles, fromIndex, toIndex);
	if(system.m_RotationBySpeedModule->GetEnabled())
		system.m_RotationBySpeedModule->Update (roState, state, particles, fromIndex, toIndex);

	Assert(count >= toIndex);
	Assert(particles.array_size() == count);
}

void ParticleSystem::UpdateModulesPostSimulationIncremental (const ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& particles, const size_t fromIndex, float dt)
{
	const size_t count = particles.array_size();
	if(system.m_SubModule->GetEnabled())
		system.m_SubModule->Update (roState, state, particles, fromIndex, particles.array_size(), dt);
	Assert(count == particles.array_size());

	if(system.m_CollisionModule->GetEnabled())
	{
#if !ENABLE_MULTITHREADED_PARTICLES
		PROFILER_AUTO(gParticleSystemUpdateCollisions,  NULL)
#endif
		system.m_CollisionModule->Update (roState, state, particles, fromIndex, dt);
	}
}

void ParticleSystem::UpdateModulesNonIncremental (const ParticleSystem& system, const ParticleSystemParticles& particles, ParticleSystemParticlesTempData& psTemp, size_t fromIndex, size_t toIndex)
{
	Assert(particles.array_size() == psTemp.particleCount);

	for(int i = fromIndex; i < toIndex; i++)
		psTemp.color[i] = particles.color[i];
	for(int i = fromIndex; i < toIndex; i++)
		psTemp.size[i] = particles.size[i];

	if(system.m_ColorModule->GetEnabled())
		system.m_ColorModule->Update (particles, psTemp.color, fromIndex, toIndex);
	if(system.m_ColorBySpeedModule->GetEnabled())
		system.m_ColorBySpeedModule->Update (particles, psTemp.color, fromIndex, toIndex);
	if(system.m_SizeModule->GetEnabled())
		system.m_SizeModule->Update (particles, psTemp.size, fromIndex, toIndex);
	if(system.m_SizeBySpeedModule->GetEnabled())
		system.m_SizeBySpeedModule->Update (particles, psTemp.size, fromIndex, toIndex);

	if (gGraphicsCaps.needsToSwizzleVertexColors)
		std::transform(&psTemp.color[fromIndex], &psTemp.color[toIndex], &psTemp.color[fromIndex], SwizzleColorForPlatform);

	if(system.m_UVModule->GetEnabled())
	{
		// No other systems used sheet index yet, allocate!
		if(!psTemp.sheetIndex)
		{
			psTemp.sheetIndex = ALLOC_TEMP_MANUAL(float, psTemp.particleCount);
			for(int i = 0; i < fromIndex; i++)
				psTemp.sheetIndex[i] = 0.0f;
		}
		system.m_UVModule->Update (particles, psTemp.sheetIndex, fromIndex, toIndex);
	}
	else if(psTemp.sheetIndex) // if this is present with disabled module, that means we have a combined buffer with one system not using UV module, just initislize to 0.0f
		for(int i = fromIndex; i < toIndex; i++)
			psTemp.sheetIndex[i] = 0.0f;
}

void ParticleSystem::UpdateModulesIncremental (const ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& particles, size_t fromIndex, float dt)
{
	UpdateModulesPreSimulationIncremental (system, roState, state, particles, fromIndex, particles.array_size(), dt);
	SimulateParticles(roState, state, particles, fromIndex, dt);
	UpdateModulesPostSimulationIncremental (system, roState, state, particles, fromIndex, dt);
}

void ParticleSystem::Update0 (ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, float dt, bool fixedTimeStep)
{
	const Transform& transform = system.GetComponent (Transform);
	Vector3f oldPosition = state.localToWorld.GetPosition();
	state.localToWorld = transform.GetLocalToWorldMatrixNoScale ();
	Matrix4x4f::Invert_General3D(state.localToWorld, state.worldToLocal);
	
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_1_a1))
		state.emitterScale = transform.GetWorldScaleLossy();
	else
		state.emitterScale = transform.GetLocalScale();
	
	if (state.playing && (dt > 0.0001f))
	{
		const Vector3f position = transform.GetPosition();

		if (roState.useLocalSpace)
			state.emitterVelocity = Vector3f::zero;
		else
			state.emitterVelocity = (position - oldPosition) / dt;
	}

	AddStagingBuffer(system);

	ParticleSystemRenderer* renderer = system.QueryComponent(ParticleSystemRenderer);
	if(renderer && !renderer->GetScreenSpaceRotation())
		ParticleSystemRenderer::SetUsesAxisOfRotationRec(system, true);

	if(system.m_RotationModule->GetEnabled() || system.m_RotationBySpeedModule->GetEnabled())
		system.SetUsesRotationalSpeed();

	int subEmitterBirthTypeCount = system.m_SubModule->GetSubEmitterTypeCount(kParticleSystemSubTypeBirth);
	if(system.m_SubModule->GetEnabled() && subEmitterBirthTypeCount)
		system.SetUsesEmitAccumulator (subEmitterBirthTypeCount);

	int subEmitterCount = SetupSubEmitters(system, *system.m_State);

#if ENABLE_MULTITHREADED_PARTICLES
	if(state.recordSubEmits)
	{
		const int numCommands = min(ParticleSystem::CalculateMaximumSubEmitterEmitCount(system, *system.m_State, dt, fixedTimeStep) * subEmitterCount, MAX_NUM_SUB_EMIT_CMDS);
		ParticleSystemSubEmitCmdBuffer& buffer = system.m_State->subEmitterCommandBuffer;
		Assert(NULL == buffer.commands);
		buffer.commands = ALLOC_TEMP_MANUAL(SubEmitterEmitCommand, numCommands);
		buffer.commandCount = 0;
		buffer.maxCommandCount = numCommands;
	}
#endif

	if(system.m_CollisionModule->GetEnabled())
		system.m_CollisionModule->AllocateAndCache(roState, state);
	if(system.m_ExternalForcesModule->GetEnabled())
		system.m_ExternalForcesModule->AllocateAndCache(roState, state);
}

void ParticleSystem::Update1 (ParticleSystem& system, ParticleSystemParticles& ps, float dt, bool fixedTimeStep, bool useProcedural, int rayBudget)
{
	PROFILER_AUTO(gParticleSystemJobProfile, NULL)
	
	const ParticleSystemReadOnlyState& roState = *system.m_ReadOnlyState;
	ParticleSystemState& state = *system.m_State;
	state.rayBudget = rayBudget;

	// Exposed through script
	dt *= std::max<float> (roState.speed, 0.0f);

	float timeStep = GetTimeStep(dt, fixedTimeStep);
	if(timeStep < 0.00001f)
		return;

	if (state.playing)
	{
		state.accumulatedDt += dt;

		if(system.GetIsDistanceEmitter())
		{
			float t = state.t + state.accumulatedDt;
			const float length = roState.lengthInSec;
			t = roState.looping ? fmodf(t, length) : min(t, length);
			size_t numContinuous = 0;
			size_t amountOfParticlesToEmit = system.EmitFromModules (system, roState, state.emissionState, numContinuous, state.emitterVelocity, state.t, t, dt);
			StartParticles(system, ps, state.t, t, dt, numContinuous, amountOfParticlesToEmit, 0.0f);
		}

		Update1Incremental(system, roState, state, ps, 0, timeStep, useProcedural);

		if (useProcedural)
			UpdateProcedural(system, roState, state, ps);
	}

	UpdateBounds(system, ps, state);
}

void ParticleSystem::Update2 (ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, bool fixedTimeStep)
{
	if(state.subEmitterCommandBuffer.commandCount > 0)
		PlaybackSubEmitterCommandBuffer(system, state, fixedTimeStep);
	state.ClearSubEmitterCommandBuffer();

	CollisionModule::FreeCache(state);
	ExternalForcesModule::FreeCache(state);

	AddStagingBuffer(system);

	//
	// Update renderer
	ParticleSystemRenderer* renderer = system.QueryComponent(ParticleSystemRenderer);
	if (renderer)
	{
		MinMaxAABB result;
		ParticleSystemRenderer::CombineBoundsRec(system, result, true);
		renderer->Update (result);
	}
}

// Returns true if update loop is executed at least once
void ParticleSystem::Update1Incremental(ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps, size_t fromIndex, float dt, bool useProcedural)
{
	ApplyStartDelay (state.delayT, state.accumulatedDt);

	int numTimeSteps = 0;
	const int numTimeStepsTotal = int(state.accumulatedDt / dt);

	while (state.accumulatedDt >= dt)
	{
		const float prevT = state.t;
		state.Tick (roState, dt);
		const float t = state.t;
		const bool timePassedDuration = t >= (roState.lengthInSec);
		const float frameOffset = float(numTimeStepsTotal - 1 - numTimeSteps);

		if(!roState.looping && timePassedDuration)
			system.Stop();

		// Update simulation
		if (!useProcedural)
			UpdateModulesIncremental(system, roState, state, ps, fromIndex, dt);
		else
			for (int i=0;i<state.emitReplay.size();i++)
				state.emitReplay[i].aliveTime += dt;

		// Emission
		bool emit = !system.GetIsDistanceEmitter() && !state.stopEmitting;
		if(emit)
		{
			size_t numContinuous = 0;
			size_t amountOfParticlesToEmit = system.EmitFromModules (system, roState, state.emissionState, numContinuous, state.emitterVelocity, prevT, t, dt);
			if(useProcedural)
				StartParticlesProcedural(system, ps, prevT, t, dt, numContinuous, amountOfParticlesToEmit, frameOffset);
			else
				StartParticles(system, ps, prevT, t, dt, numContinuous, amountOfParticlesToEmit, frameOffset);
		}

		state.accumulatedDt -= dt;

		AddStagingBuffer(system);

		// Workaround for external forces being dependent on AABB (need to update it before the next time step)
		if(!useProcedural && (state.accumulatedDt >= dt) && system.m_ExternalForcesModule->GetEnabled())
			UpdateBounds(system, ps, state);

		numTimeSteps++;
	}
}

void ParticleSystem::UpdateProcedural (ParticleSystem& system, const ParticleSystemReadOnlyState& roState, ParticleSystemState& state, ParticleSystemParticles& ps)
{
	DebugAssert(CheckSupportsProcedural(system));

	// Clear all particles
	ps.array_resize(0);

	const Matrix4x4f localToWorld = !roState.useLocalSpace ? state.localToWorld : Matrix4x4f::identity;

	// Emit all particles
	for (int i=0; i<state.emitReplay.size(); i++)
	{
		const ParticleSystemEmitReplay& emit = state.emitReplay[i];

#if UNITY_EDITOR
		ParticleSystemEditor::ApplyRandomSeed(system, emit.randomSeed);
#endif
		//@TODO: remove passing m_State since that is very dangerous when making things procedural compatible
		size_t previousParticleCount = ps.array_size();
		system.m_InitialModule.GenerateProcedural (roState, state, ps, emit);

		//@TODO: This can be moved out of the emit all particles loop...
		if (system.m_ShapeModule.GetEnabled())
			system.m_ShapeModule.Start (roState, state, ps, localToWorld, previousParticleCount, emit.t);

		// Apply gravity & integrated velocity after shape module so that it picks up any changes done in shapemodule (for example rotating the velocity)
		Vector3f gravity = system.m_InitialModule.GetGravity(roState, state);
		float particleIndex = 0.0f;
		const size_t particleCount = ps.array_size();
		for (int q = previousParticleCount; q < particleCount; q++)
		{
			const float normalizedT = emit.t / roState.lengthInSec;
			ps.velocity[q] *= Evaluate (system.m_InitialModule.GetSpeedCurve(), normalizedT, GenerateRandom(ps.randomSeed[q] + kParticleSystemStartSpeedCurveId));
			Vector3f velocity = ps.velocity[q];
			float frameOffset = (particleIndex + emit.emissionOffset) * emit.emissionGap * float(particleIndex < emit.numContinuous);
			float aliveTime = emit.aliveTime + frameOffset;

			ps.position[q] += velocity * aliveTime + gravity * aliveTime * aliveTime * 0.5F;
			ps.velocity[q] += gravity * aliveTime;

			particleIndex += 1.0f;
		}

		// If no particles were emitted we can get rid of the emit replay state...
		if (previousParticleCount == ps.array_size())
		{
			state.emitReplay[i] = state.emitReplay.back();
			state.emitReplay.pop_back();
			i--;
		}
	}

	if (system.m_RotationModule->GetEnabled())
		system.m_RotationModule->UpdateProcedural (state, ps);

	if (system.m_VelocityModule->GetEnabled())
		system.m_VelocityModule->UpdateProcedural (roState, state, ps);

	if (system.m_ForceModule->GetEnabled())
		system.m_ForceModule->UpdateProcedural (roState, state, ps);

	// Modules that are not supported by procedural
	DebugAssert(!system.m_RotationBySpeedModule->GetEnabled()); // find out if possible to support it
	DebugAssert(!system.m_ClampVelocityModule->GetEnabled()); // unsupported: (Need to compute velocity by deriving position curves...), possible to support?
	DebugAssert(!system.m_CollisionModule->GetEnabled());
	DebugAssert(!system.m_SubModule->GetEnabled()); // find out if possible to support it
	DebugAssert(!system.m_ExternalForcesModule->GetEnabled());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Editor only
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if UNITY_EDITOR
void ParticleSystem::TransformChanged()
{
	if(!IsWorldPlaying() && ParticleSystemEditor::GetResimulation())
	{
		const Transform& transform = GetComponent (Transform);
		Matrix4x4f newMatrix = transform.GetLocalToWorldMatrixNoScale ();
		newMatrix.SetPosition(Vector3f::zero);
		Matrix4x4f oldMatrix = m_State->localToWorld;
		oldMatrix.SetPosition(Vector3f::zero);
		bool rotationChanged = !CompareApproximately(oldMatrix, newMatrix);
		if(m_ReadOnlyState->useLocalSpace || rotationChanged)
			ParticleSystemEditor::PerformCompleteResimulation(this);
	}
}
#endif

