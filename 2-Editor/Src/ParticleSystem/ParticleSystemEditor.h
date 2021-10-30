#ifndef PARTICLESYSTEMEDITOR_H
#define PARTICLESYSTEMEDITOR_H

#include "Runtime/Graphics/ParticleSystem/ParticleSystemCommon.h"
#include "Runtime/Utilities/LinkedList.h"

class ParticleSystem;
typedef List< ListNode<ParticleSystem> > ParticleSystemList;

class ParticleSystemEditor
{
public:
	static void Initialize ();

	static void SetupDefaultParticleSystem(ParticleSystem& system);
	static void SetupDefaultParticleSystemType(ParticleSystem& system, ParticleSystemSubType type);

	static UInt32 LookupRandomSeed(int offset);
	static void UpdateRandomSeed(ParticleSystem& system);
	static void ApplyRandomSeed(ParticleSystem& system, UInt32 randomSeed);
	static UInt32 GetRandomSeed(ParticleSystem& system);
	
	static void SetPlaybackTime(float value);
	static float GetPlaybackTime();

	static void SetPlaybackIsPlaying (bool value);
	static bool GetPlaybackIsPlaying ();

	static void SetPlaybackIsPaused (bool value);
	static bool GetPlaybackIsPaused ();

	static bool GetPlaybackIsStopped() { return !GetPlaybackIsPlaying() && !GetPlaybackIsPaused(); }

	static void SetPerformCompleteResimulation(bool value);
	static bool GetPerformCompleteResimulation();
	static void  SetSimulationSpeed (float speed);
	static float GetSimulationSpeed ();	
	static void  SetIsScrubbing (bool value);
	static bool GetIsScrubbing();
	static bool GetIsExtraInterpolationStep();
	static void SetResimulation (bool value);
	static bool GetResimulation ();
	static void SetUpdateAll(bool value);
	static bool GetUpdateAll();
	static void SetLockedParticleSystem (ParticleSystem* particleSystem);
	static ParticleSystem* GetLockedParticleSystem();

	static void TagAllSubEmitters(ParticleSystemList& emitters, ParticleSystem* system, bool value);

	static void UpdatePreview(ParticleSystem* system, float deltaTime);
	static void Update2Recurse(ParticleSystem* system);

	static void PerformCompleteResimulation(ParticleSystem* system);
	static void PerformInterpolationStep(ParticleSystem* system);
	static void StopAndClearSubEmittersRecurse(ParticleSystem* system);

	static void CollectSubEmittersRec (ParticleSystem* system, ParticleSystemList& emitters);
	static void UpdateAll ();
	static void CollectSubEmittersRec(ParticleSystem* system, std::vector<ParticleSystem*>& subEmitters);
	static bool UseInterpolation(const ParticleSystem& system);

	static void GetSubEmitterPtrs(ParticleSystem& system, ParticleSystem** outSystems);
};

#endif // PARTICLESYSTEMEDITOR_H
