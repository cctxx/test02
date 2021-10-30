#include "UnityPrefix.h"
#include "MuscleClipQualityInfo.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/mecanim/animation/avatar.h"

MuscleClipQualityInfo GetMuscleClipQualityInfo (AnimationClip& clip, float startTime, float stopTime) 
{ 
	MuscleClipQualityInfo info;
	
	info.m_Loop = 0;
	info.m_LoopOrientation = 0;
	info.m_LoopPositionY = 0;
	info.m_LoopPositionXZ = 0;
	
	mecanim::animation::ClipMuscleConstant* muscleClip = clip.GetRuntimeAsset();
	if (muscleClip == NULL)
		return info;
	
	mecanim::human::HumanPose deltaPose;
	math::xform startX;
	math::xform leftFootStartX;
	math::xform rightFootStartX;
	
	mecanim::memory::MecanimAllocator alloc(kMemTempAlloc);
	mecanim::animation::ComputeClipMuscleDeltaPose(*muscleClip,startTime,stopTime,deltaPose,startX,leftFootStartX,rightFootStartX, alloc);
	
	info.m_Loop = mecanim::human::DeltaPoseQuality(deltaPose);
	info.m_LoopOrientation = 1.0f - 2.0f * math::length(math::quat2Qtan(deltaPose.m_RootX.q)).tofloat();
	
	math::xform stopX = math::xformMulInv(startX,deltaPose.m_RootX);
	math::float4 diff = stopX.t - startX.t;
	
	info.m_LoopPositionY = (0.1f - math::abs(diff.y().tofloat()))/0.1f;
	
	diff.y() = 0;
	
	info.m_LoopPositionXZ = (0.1f - math::length(diff).tofloat())/0.1f;
	
	return info;
}

void CalculateQualityCurves (AnimationClip& clip, float fixedTime, float variableEndStart, float variableEndEnd, int q, int sampleCount, Vector2f* poseCurve, Vector2f* rotationCurve, Vector2f* heightCurve, Vector2f* positionCurve)
{
	mecanim::animation::ClipMuscleConstant* muscleClip = clip.GetRuntimeAsset();
	if (muscleClip == NULL)
		return;
	
	const int kSamplesPerSecond = 60;
	
	// Start sample may be a bit before start time; stop sample may be a bit after stop time
	int startSample = static_cast<int>(std::floor(variableEndStart * kSamplesPerSecond));
	int stopSample = static_cast<int>(std::ceil(variableEndEnd * kSamplesPerSecond));
	
	mecanim::memory::MecanimAllocator alloc(kMemTempAlloc);
	mecanim::animation::ClipInput input;
	mecanim::human::HumanPose poseOutFixed;
	mecanim::human::HumanPose poseOut;
	mecanim::animation::ClipOutput *clipOutFixed = mecanim::animation::CreateClipOutput(muscleClip->m_Clip.Get(), alloc);
	mecanim::animation::ClipOutput *clipOut = mecanim::animation::CreateClipOutput(muscleClip->m_Clip.Get(), alloc);
	mecanim::animation::ClipMemory *mem = mecanim::animation::CreateClipMemory(muscleClip->m_Clip.Get(), alloc);
	
	input.m_Time = fixedTime;
	mecanim::animation::EvaluateClip(muscleClip->m_Clip.Get(),&input,mem,clipOutFixed);
	mecanim::animation::GetHumanPose(*muscleClip,clipOutFixed->m_Values,poseOutFixed);
	
	mecanim::human::HumanPose deltaPose;
	
	Assert(sampleCount >= stopSample-startSample);
	for (int i=0; i<sampleCount; i++)
	{
		// Clamp sample time so first and last sample are exactly on start and stop time respectively
		float time = math::clamp ( static_cast<float>(startSample+i) / kSamplesPerSecond, variableEndStart, variableEndEnd);
		
		input.m_Time = time;
		mecanim::animation::EvaluateClip(muscleClip->m_Clip.Get(),&input,mem,clipOut);
		mecanim::animation::GetHumanPose(*muscleClip,clipOut->m_Values,poseOut);
		
		mecanim::human::HumanPose& startPose = q == 0 ? poseOut : poseOutFixed;
		mecanim::human::HumanPose& stopPose = q == 0 ? poseOutFixed : poseOut;
		
		mecanim::human::HumanPoseSub(deltaPose,startPose,stopPose);
		
		poseCurve[i] = Vector2f(time, math::saturate(1 - mecanim::human::DeltaPoseQuality(deltaPose)));
		rotationCurve[i] = Vector2f(time, math::saturate (1.f - (1.f - 2.f * math::length(math::quat2Qtan(deltaPose.m_RootX.q)).tofloat() )));
		
		math::xform stopX = math::xformMulInv(startPose.m_RootX, deltaPose.m_RootX);
		math::float4 diff = stopX.t - startPose.m_RootX.t;
		
		heightCurve[i] = Vector2f(time, math::saturate (1.f - ((0.1f - math::abs( diff.y().tofloat() ))/0.1f)));
		
		diff.y() = 0;
		
		positionCurve[i] = Vector2f(time, math::saturate (1.f - ((0.1f - math::length(diff).tofloat() )/0.1f)));
	}
	
	DestroyClipOutput (clipOutFixed,alloc);
	DestroyClipOutput (clipOut,alloc);
	DestroyClipMemory (mem,alloc);
}

