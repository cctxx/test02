#include "UnityPrefix.h"
#include "ParticleSystemEditor.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemRenderer.h"
#include "Runtime/Graphics/ParticleSystem/Modules/CollisionModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/ColorModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/ColorByVelocityModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/ForceModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/EmissionModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/RotationModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/RotationByVelocityModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/SizeModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/SizeByVelocityModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/SubModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/UVModule.h"
#include "Runtime/Graphics/ParticleSystem/Modules/VelocityModule.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Misc/QualitySettings.h"

static const char* kResimulation = "Shuriken-Resimulation";

namespace 
{
	struct ParticleSystemEditorState
	{
		enum { kNumRandomSeeds = 2048 };
		
		ParticleSystemList activeEmitters;
		float playbackTime;
		float previousPlaybackTime;
		float simulationSpeed;
		UInt32 randomSeedTable[kNumRandomSeeds];
		bool isScrubbing;
		bool playbackIsPlaying;
		bool playbackIsPaused;
		bool performCompleteResimulation;
		bool isExtraInterpolationStep;
		bool resimulation;
		bool updateAll;
		PPtr<ParticleSystem> lockedParticleSystem;
		
		ParticleSystemEditorState ()
		:playbackTime(0.0f)
		,previousPlaybackTime(0.0f)
		,simulationSpeed(1.0f)
		,isScrubbing(false)
		,playbackIsPlaying(false)
		,playbackIsPaused(false)
		,performCompleteResimulation(false)
		,isExtraInterpolationStep(false)
		,resimulation(true)
		,updateAll(false)
		{
			Rand rand (0x1337);
			for(int i = 0; i < kNumRandomSeeds; i++)
				randomSeedTable[i] = RangedRandom(rand, 0, 30000);
		}
	};
	ParticleSystemEditorState gParticleSystemEditorState;
}

 void ParticleSystemEditor::Initialize ()
{
	gParticleSystemEditorState.resimulation = EditorPrefs::GetBool (kResimulation, true);
}

UInt32 ParticleSystemEditor::LookupRandomSeed(int index)
{
	while(index < 0)
		index += ParticleSystemEditorState::kNumRandomSeeds;
	return gParticleSystemEditorState.randomSeedTable[index % ParticleSystemEditorState::kNumRandomSeeds];
}

void ParticleSystemEditor::SetPlaybackTime(float value)
{
	gParticleSystemEditorState.playbackTime = clamp(value, 0.0f, 999.0f);
}

float ParticleSystemEditor::GetPlaybackTime()
{
	return gParticleSystemEditorState.playbackTime;
}

void ParticleSystemEditor::SetPlaybackIsPlaying (bool value)
{
	gParticleSystemEditorState.playbackIsPlaying = value;
}

bool ParticleSystemEditor::GetPlaybackIsPlaying ()
{
	return gParticleSystemEditorState.playbackIsPlaying;
}

void ParticleSystemEditor::SetPlaybackIsPaused (bool value)
{
	gParticleSystemEditorState.playbackIsPaused = value;
}

bool ParticleSystemEditor::GetPlaybackIsPaused ()
{
	return gParticleSystemEditorState.playbackIsPaused;
}

void ParticleSystemEditor::SetPerformCompleteResimulation(bool value)
{
	gParticleSystemEditorState.performCompleteResimulation = value;
}

bool ParticleSystemEditor::GetPerformCompleteResimulation()
{
	return gParticleSystemEditorState.performCompleteResimulation;
}

void ParticleSystemEditor::SetSimulationSpeed (float speed)
{
	gParticleSystemEditorState.simulationSpeed = speed;
}

float ParticleSystemEditor::GetSimulationSpeed ()
{
	return gParticleSystemEditorState.simulationSpeed;
}

void ParticleSystemEditor::SetIsScrubbing (bool value)
{
	gParticleSystemEditorState.isScrubbing = value;
}

bool ParticleSystemEditor::GetIsScrubbing()
{
	return gParticleSystemEditorState.isScrubbing;
}

bool ParticleSystemEditor::GetIsExtraInterpolationStep()
{
	return gParticleSystemEditorState.isExtraInterpolationStep;
}

void ParticleSystemEditor::SetResimulation (bool value)
{
	if(value != gParticleSystemEditorState.resimulation)
	{
		gParticleSystemEditorState.resimulation = value;
		SetPerformCompleteResimulation(true);
		EditorPrefs::SetBool (kResimulation, value);
	}
}

bool ParticleSystemEditor::GetResimulation ()
{
	return gParticleSystemEditorState.resimulation;
}

void ParticleSystemEditor::SetUpdateAll (bool updateAll)
{
// 	if (IsWorldPlaying())
// 		return;
// 
// 	if (updateAll)
// 	{
// 		if (gParticleSystemEditorState.activeEmitters.empty())
// 		{
// 			std::vector<ParticleSystem*> systems;
// 			Object::FindObjectsOfType (&systems);
// 			const int count = systems.size();
// 			for(int i = 0; i < count; i++)
// 			{
// 				ParticleSystem *system = systems[i];
// 				if (system->GetPlayOnAwake())
// 				{
// 					if (!system->IsPlaying())
// 						system->Play ();
// 					gParticleSystemEditorState.activeEmitters.push_back (system->m_EditorListNode);
// 				}
// 			}
// 		}
// 	}
// 	else
// 	{
// 		if (!gParticleSystemEditorState.activeEmitters.empty())
// 		{
// 			ParticleSystemList& emitters = gParticleSystemEditorState.activeEmitters;
// 			for (ParticleSystemList::iterator i = emitters.begin(); i != emitters.end(); i++)
// 			{
// 				ParticleSystem& system = **i;
// 				if (system.IsPlaying())
// 				{	
// 					system.Stop ();
// 					system.Clear ();
// 				}
// 			}
// 			gParticleSystemEditorState.activeEmitters.clear ();
// 		}
// 	}
}

bool ParticleSystemEditor::GetUpdateAll()
{
	return false;
	//return !gParticleSystemEditorState.activeEmitters.empty ();
}


void ParticleSystemEditor::SetLockedParticleSystem (ParticleSystem* particleSystem)
{
	gParticleSystemEditorState.lockedParticleSystem = particleSystem;
}

ParticleSystem* ParticleSystemEditor::GetLockedParticleSystem ()
{
	ParticleSystem* particleSystem = gParticleSystemEditorState.lockedParticleSystem;
	return particleSystem;
}

void ParticleSystemEditor::StopAndClearSubEmittersRecurse(ParticleSystem* system)
{
	system->Stop();
	system->Clear();
	ParticleSystem* subs[kParticleSystemMaxSubTotal];
	system->m_SubModule->GetSubEmitterPtrs(&subs[0]);
	SubModule::RemoveDuplicatePtrs(&subs[0]);
	for(int i = 0; i < kParticleSystemMaxSubTotal; i++)
		if(subs[i] && (subs[i] != system))
			ParticleSystemEditor::StopAndClearSubEmittersRecurse(subs[i]);
}

void ParticleSystemEditor::UpdatePreview(ParticleSystem* system, float deltaTime)
{
	// Collect all systems in list
	ParticleSystemList emitters;
	ParticleSystemEditor::CollectSubEmittersRec (system, emitters);
	SafeIterator< ParticleSystemList > i (emitters);

	if(!IsWorldPlaying())
		TagAllSubEmitters(emitters, system, true);

	while (i.Next())
	{
		ParticleSystem& system = **i;
		if(!system.m_State->culled)
			if(deltaTime >= 0.0001f)
			{
				// We have to work out a budget for raycasting here. At this point we only update locked and selected particle systems, so we can't really dole out the 
				// budget like we do in the player where we have the whole picture. So this will most probably get a higher budget than in the player version where the 
				// global budget is shared among all the active particle systems. We could obviously share the budget between the selected particle systems here but its 
				// going to be quite approximate anyway.
				const bool approximate = ( system.m_CollisionModule && system.m_CollisionModule->IsApproximate() );
				const int rayBudget = ( approximate ? std::min((int)system.GetParticleCount(),GetQualitySettings().GetCurrent().particleRaycastBudget) : (int)system.GetParticleCount());
				ParticleSystem::Update(system, deltaTime, true, false, rayBudget );
			}
	}
	
	if(!IsWorldPlaying())
		TagAllSubEmitters(emitters, system, false);
}

void ParticleSystemEditor::PerformCompleteResimulation(ParticleSystem* system)
{
	if (system->IsStopped())
		return;

	// Collect all systems in list
	ParticleSystemList emitters;
	ParticleSystemEditor::CollectSubEmittersRec (system, emitters);

	if(!IsWorldPlaying())
		TagAllSubEmitters(emitters, system, true);

	const bool orgPlaying = system->m_State->playing;
	ParticleSystemEditor::StopAndClearSubEmittersRecurse(system);
	system->m_State->t = 0.0f;
	system->m_State->accumulatedDt = 0.0f;
	system->m_State->emissionState.Clear();

	system->Simulate (ParticleSystemEditor::GetPlaybackTime(), true);
	system->m_State->playing = orgPlaying;
	system->m_State->invalidateProcedural = false;

	if(!IsWorldPlaying())
		TagAllSubEmitters(emitters, system, false);
}

void ParticleSystemEditor::PerformInterpolationStep(ParticleSystem* system)
{
	if(!UseInterpolation(*system))
		return;

	ParticleSystemReadOnlyState& roState = *system->m_ReadOnlyState;
	ParticleSystemState& state = *system->m_State;
	CollisionModule* collisionModule = system->m_CollisionModule;
	SubModule* subModule = system->m_SubModule;
	ParticleSystemParticles& ps0 = system->m_Particles[ParticleSystem::kParticleBuffer0];
	ParticleSystemParticles& ps1 = system->m_Particles[ParticleSystem::kParticleBuffer1];

	ps1.array_assign(ps0);

	// Save states
	const size_t orgNumSubCollision = state.numCachedSubDataCollision;
	const float orgCollisionLifetimeLoss = collisionModule->GetEnergyLoss();
	const float orgCollisionMinKillSpeed = collisionModule->GetMinKillSpeed();
	const bool orgIsSubEnabled = subModule->GetEnabled();
	const float orgAccumulatedDt = state.accumulatedDt;
	const float orgT = state.t;
	const bool orgIsPlaying = state.playing;
	
	// Disallow:
	// 1. New emits 
	// 2. Particles dying
	// 3. Change of order (sorting etc.)
	// ... as Lerp() required array size & order to be the same between time steps 
	state.numCachedSubDataCollision = 0;
	state.playing = true;
	
	collisionModule->SetEnergyLoss(0.0f);
	collisionModule->SetMinKillSpeed(0.0f);
	subModule->SetEnabled(false);
	gParticleSystemEditorState.isExtraInterpolationStep = true;

	const float fixedTimeStep = GetTimeManager().GetFixedDeltaTime();
	const size_t numParticlesPreUpdate = ps1.array_size();
	ParticleSystem::UpdateModulesIncremental (*system, roState, state, ps1, 0, fixedTimeStep);
	DebugAssert(orgAccumulatedDt == state.accumulatedDt);
	DebugAssert(orgT == state.t);
	DebugAssert(numParticlesPreUpdate == ps1.array_size());

	// Restore state
	state.numCachedSubDataCollision = orgNumSubCollision;
	state.playing = orgIsPlaying;
	collisionModule->SetEnergyLoss(orgCollisionLifetimeLoss);
	collisionModule->SetMinKillSpeed(orgCollisionMinKillSpeed);
	subModule->SetEnabled(orgIsSubEnabled);
	gParticleSystemEditorState.isExtraInterpolationStep = false;

	// Lerp
	const float factor = orgAccumulatedDt / fixedTimeStep;
	ParticleSystemParticles::array_lerp(ps1, ps0, ps1, factor);
}

void ParticleSystemEditor::TagAllSubEmitters(ParticleSystemList& emitters, ParticleSystem* system, bool value)
{
	for (ParticleSystemList::iterator i = emitters.begin(); i != emitters.end(); i++)
		if(i->GetData() != system) // don't tag self
			(**i).m_State->SetIsSubEmitter(value);
}

void ParticleSystemEditor::UpdateAll ()
{
	const float deltaTimeEpsilon = 0.0001f;
	float deltaTime = GetDeltaTime();
	if(deltaTime < deltaTimeEpsilon)
		return;

	for (ParticleSystemList::iterator i = gParticleSystemEditorState.activeEmitters.begin(); i != gParticleSystemEditorState.activeEmitters.end(); i++)
		ParticleSystem::Update(**i, deltaTime, false, false);
}

// this shit: instead just set 
void ParticleSystemEditor::CollectSubEmittersRec (ParticleSystem* system, List< ListNode<ParticleSystem> >& emitters)
{
	emitters.push_back (system->m_EditorListNode);
	ParticleSystem* subEmitters[kParticleSystemMaxSubTotal];
	system->m_SubModule->GetSubEmitterPtrs(&subEmitters[0]);
	SubModule::RemoveDuplicatePtrs(&subEmitters[0]);
	for(int i = 0; i < kParticleSystemMaxSubTotal; i++)
		if(subEmitters[i] && subEmitters[i] != system)
			CollectSubEmittersRec(subEmitters[i], emitters);
}

void ParticleSystemEditor::CollectSubEmittersRec(ParticleSystem* system, std::vector<ParticleSystem*>& subEmitters)
{
	ParticleSystem* systems[kParticleSystemMaxSubTotal];
	system->m_SubModule->GetSubEmitterPtrs(&systems[0]);
	SubModule::RemoveDuplicatePtrs(&systems[0]);

	for(int i = 0; i < kParticleSystemMaxSubTotal; i++)
	{
		ParticleSystem* system = systems[i];
		if(system && count (subEmitters.begin(), subEmitters.end(), system) == 0)
		{
			subEmitters.push_back(system);
			CollectSubEmittersRec(system, subEmitters);
		}
	}
}

bool ParticleSystemEditor::UseInterpolation(const ParticleSystem& system)
{
	return !IsWorldPlaying ();
}

void ParticleSystemEditor::SetupDefaultParticleSystem(ParticleSystem& system)
{
	system.m_InitialModule.GetLifeTimeCurve().SetScalar(5.0f);
	system.m_InitialModule.GetSpeedCurve().SetScalar(5.0f);
	system.m_InitialModule.GetRotationCurve().SetScalar(0.0f);
	
	system.m_ColorModule->GetGradient().minMaxState = kMMGGradient;
	system.m_ColorBySpeedModule->GetGradient().minMaxState = kMMGGradient;
	system.m_SizeModule->GetCurve().minMaxState = kMMCCurve;
	system.m_SizeBySpeedModule->GetCurve().minMaxState = kMMCCurve;
	
	system.m_UVModule->GetCurve().minMaxState = kMMCCurve;
	SetPolynomialCurveToLinear(system.m_UVModule->GetCurve().editorCurves.max, system.m_UVModule->GetCurve().polyCurves.max);
	SetPolynomialCurveToLinear(system.m_UVModule->GetCurve().editorCurves.min, system.m_UVModule->GetCurve().polyCurves.min);
	
	system.m_ForceModule->GetXCurve().SetScalar(0.0f);
	system.m_ForceModule->GetYCurve().SetScalar(0.0f);
	system.m_ForceModule->GetZCurve().SetScalar(0.0f);
	system.m_RotationModule->GetCurve().SetScalar(Deg2Rad(45.0f));
	system.m_RotationBySpeedModule->GetCurve().SetScalar(Deg2Rad(45.0f));
	system.m_VelocityModule->GetXCurve().SetScalar(0.0f);
	system.m_VelocityModule->GetYCurve().SetScalar(0.0f);
	system.m_VelocityModule->GetZCurve().SetScalar(0.0f);

	system.m_EmissionModule.GetEmissionData().rate.SetScalar(10.0f);
}


void ParticleSystemEditor::SetupDefaultParticleSystemType(ParticleSystem& system, ParticleSystemSubType type)
{
	if (kParticleSystemSubTypeBirth == type)
	{
		system.m_ReadOnlyState->prewarm = false;
		system.m_InitialModule.GetLifeTimeCurve().SetScalar(1.0f);
		system.m_InitialModule.GetSpeedCurve().SetScalar(0.0f);
		system.m_InitialModule.GetSizeCurve().SetScalar(0.5f);
		system.m_EmissionModule.GetEmissionData().rate.SetScalar(10.0f);
		system.m_EmissionModule.GetEmissionData().type = EmissionModule::kEmissionTypeTime;
		system.m_ShapeModule.SetShapeType((int)ShapeModule::kSphere);
		system.m_ShapeModule.SetRadius(0.0f);
	}
	else if (kParticleSystemSubTypeCollision == type ||
		kParticleSystemSubTypeDeath == type)
	{
		system.m_ReadOnlyState->prewarm = false;
		system.m_InitialModule.GetSpeedCurve().SetScalar(1.0f);
		system.m_InitialModule.GetSizeCurve().SetScalar(0.5f);
		system.m_EmissionModule.GetEmissionData().rate.SetScalar(0.0f);
		system.m_EmissionModule.GetEmissionData().burstCount = 1;
		system.m_ShapeModule.SetShapeType((int)ShapeModule::kSphere);
		system.m_ShapeModule.SetRadius(0.0f);
	}
	else
	{
		DebugAssert(0 && "Type not yet implemented");
	}
}

void ParticleSystemEditor::UpdateRandomSeed(ParticleSystem& system)
{
	system.m_EditorRandomSeedIndex++;
}

void ParticleSystemEditor::ApplyRandomSeed(ParticleSystem& system, UInt32 seedOffset)
{
	UInt32 randomSeed = system.m_ReadOnlyState->randomSeed;
	if(randomSeed == 0)
		randomSeed = (UInt32)(&system);
	system.m_InitialModule.m_EditorRandom = seedOffset + randomSeed;
	system.m_ShapeModule.m_EditorRandom = seedOffset + randomSeed + 5;
}

UInt32 ParticleSystemEditor::GetRandomSeed(ParticleSystem& system)
{
	return LookupRandomSeed(system.m_EditorRandomSeedIndex);
}

void ParticleSystemEditor::GetSubEmitterPtrs(ParticleSystem& system, ParticleSystem** outSystems)
{
	system.m_SubModule->GetSubEmitterPtrs(outSystems);
}

