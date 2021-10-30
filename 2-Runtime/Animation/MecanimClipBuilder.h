#pragma once

#include "PPtrKeyframes.h"
#include "Runtime/Math/AnimationCurve.h"
#include "AnimationClipBindings.h"
#include "Runtime/mecanim/animation/clipmuscle.h"

/// Builds a full mecanim clip from source curve data
/// When building a mecanim clip we classify all curves into:
/// streamedclip: hermite curve polynomials
/// denseclip: linearly interpolated non-sparse keyframes
/// constantclip: value doesn't change over time


struct AnimationClipSettings;

enum ClipOptType { kInvalidCurve = -1, kStreamedClip = 0, kDenseClip, kConstantClip, kClipOptCount };

struct MecanimClipBuilder
{
	struct Curves
	{
		dynamic_array<AnimationCurveVec3*>				positionCurves;
		dynamic_array<AnimationCurveQuat*>				rotationCurves;
		dynamic_array<AnimationCurveVec3*>				scaleCurves;
		dynamic_array<AnimationCurve*>					genericCurves;
		dynamic_array<PPtrKeyframes*>					pptrCurves;
		
		size_t											totalCurveCount;
		size_t											totalKeyCount;
		
		dynamic_array<UnityEngine::Animation::GenericBinding>					bindings;
	};
	
	MecanimClipBuilder ();
	
	mecanim::uint32_t muscleIndexArray[mecanim::animation::s_ClipMuscleCurveCount];
	
	Curves	curves[kClipOptCount];
	size_t  totalBindingCount;
	size_t  totalCurveCount;
	bool    hasAnimationEvents;
	float	startTime;
	float	stopTime;
	float	sampleRate;
};

void AddPositionCurveToClipBuilder (AnimationCurveVec3& curve, const UnityStr& path, MecanimClipBuilder& clipBuilder, bool useHighQualityCurve);
void AddRotationCurveToClipBuilder (AnimationCurveQuat& curve, const UnityStr& path, MecanimClipBuilder& clipBuilder, bool useHighQualityCurve);
void AddScaleCurveToClipBuilder (AnimationCurveVec3& curve, const UnityStr& path, MecanimClipBuilder& clipBuilder, bool useHighQualityCurve);
void AddGenericCurveToClipBuilder (AnimationCurve& curve, const UnityEngine::Animation::GenericBinding& binding, MecanimClipBuilder& clipBuilder, bool useHighQualityCurve);
void AddPPtrCurveToClipBuilder (PPtrKeyframes& curve, const UnityEngine::Animation::GenericBinding& binding, MecanimClipBuilder& clipBuilder);

bool PrepareClipBuilder (MecanimClipBuilder& clipBuilder);
mecanim::animation::ClipMuscleConstant* BuildMuscleClip (const MecanimClipBuilder& clipBuilder, const AnimationClipSettings& muslceClipInfo, bool isHumanClip, UnityEngine::Animation::AnimationClipBindingConstant& outClipBindings, mecanim::memory::Allocator& allocator);

void PatchMuscleClipWithInfo (const AnimationClipSettings& clipInfo, bool isHumanoid, mecanim::animation::ClipMuscleConstant *cst);
void CstToAnimationClipSettings (mecanim::animation::ClipMuscleConstant const *cst, AnimationClipSettings &clipInfo);

template<class T>
static bool IsConstantCurve (AnimationCurveTpl<T>& curve)
{
	Assert(curve.GetKeyCount() != 0);

	KeyframeTpl<T> firstKey = curve.GetKey(0);
	for (int i=0;i<curve.GetKeyCount();i++)
	{
		if (!CompareApproximately(curve.GetKey(i).value, firstKey.value))
			return false;
		if (!CompareApproximately(curve.GetKey(i).inSlope, Zero<T> ()))
			return false;
		if (!CompareApproximately(curve.GetKey(i).outSlope, Zero<T> ()))
			return false;
	}
	
	return true;
}

template<class T>
static bool IsStepKey(KeyframeTpl<T> const& key)
{
	return !IsFinite(key.inSlope) || !IsFinite(key.outSlope);
}

template<class T>
static bool IsTooDense(KeyframeTpl<T> const& key, KeyframeTpl<T> const& previousKey, float sampleStep)
{
	float delta = std::abs(key.time - previousKey.time);

	// epsilon is too small here, use a bigger threshold
	return (delta - sampleStep) < -1e-5f /*-std::numeric_limits<float>::epsilon()*/;
}

template<class T>
static bool IsDenseCurve (AnimationCurveTpl<T> const& curve)
{
	Assert(curve.GetKeyCount() != 0);

	const float samplePerSec = 30.f;
	const float sampleStep = 1.0f/samplePerSec;

	// Remember that default curve classification is Streamed curve,
	// which are ~8 time bigger in memory than a Dense curve( ~8x = constant cost + memory const)
	// 
	std::pair<float, float> range = curve.GetRange();
	float diff = range.second - range.first;
	if(diff * samplePerSec > curve.GetKeyCount() * 8)
		return false;

	if( IsStepKey(curve.GetKey(0)) )
			return false;

	// Look for step curve, they cannot be represented by a dense curve
	for (int i=1;i<curve.GetKeyCount();i++)
	{
		KeyframeTpl<T> const& previousKey = curve.GetKey(i-1);
		KeyframeTpl<T> const& key = curve.GetKey(i);
		
		if( IsStepKey(key) )
			return false;

		// For now if there is more key than sampling rate, revert back to streamed clip.
		if( IsTooDense( key, previousKey, sampleStep) )
			return false;
	}
	
	return true;
}
