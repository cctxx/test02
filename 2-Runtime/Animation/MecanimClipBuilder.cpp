#include "UnityPrefix.h"
#include "MecanimClipBuilder.h"
#include "Runtime/mecanim/animation/clipmuscle.h"
#include "StreamedClipBuilder.h"
#include "DenseClipBuilder.h"
#include "GenericAnimationBindingCache.h"
#include "AnimationClipSettings.h"

MecanimClipBuilder::MecanimClipBuilder ()
	:hasAnimationEvents(false),
	startTime(std::numeric_limits<float>::infinity()),
	stopTime(-std::numeric_limits<float>::infinity()),
	sampleRate (30.0F)
{
	// Muscle curves
	for(mecanim::uint32_t muscleIter = 0; muscleIter < mecanim::animation::s_ClipMuscleCurveCount; muscleIter++)
		muscleIndexArray[muscleIter] = -1;
}

void PatchMuscleClipWithInfo (const AnimationClipSettings& clipInfo, bool isHumanoid, mecanim::animation::ClipMuscleConstant *cst)
{
	cst->m_StartTime = clipInfo.m_StartTime;
	cst->m_StopTime = clipInfo.m_StopTime;
	cst->m_OrientationOffsetY = clipInfo.m_OrientationOffsetY;
	cst->m_Level = clipInfo.m_Level;
	cst->m_CycleOffset = clipInfo.m_CycleOffset;
	cst->m_LoopTime = clipInfo.m_LoopTime;
	cst->m_LoopBlend = clipInfo.m_LoopBlend;
	cst->m_LoopBlendOrientation = clipInfo.m_LoopBlendOrientation;
	cst->m_LoopBlendPositionY = clipInfo.m_LoopBlendPositionY;
	cst->m_LoopBlendPositionXZ = clipInfo.m_LoopBlendPositionXZ;
	cst->m_KeepOriginalOrientation = clipInfo.m_KeepOriginalOrientation;
	cst->m_KeepOriginalPositionY = clipInfo.m_KeepOriginalPositionY;
	cst->m_KeepOriginalPositionXZ = clipInfo.m_KeepOriginalPositionXZ;
	cst->m_HeightFromFeet = clipInfo.m_HeightFromFeet;
	cst->m_Mirror = clipInfo.m_Mirror;
	
	if (isHumanoid)
	{
		mecanim::animation::InitClipMuscleDeltaPose (*cst);
		mecanim::animation::InitClipMuscleAverageSpeed (*cst);
	}
	mecanim::animation::InitClipMuscleDeltaValues (*cst);
}

void CstToAnimationClipSettings (mecanim::animation::ClipMuscleConstant const *cst, AnimationClipSettings &clipInfo)
{
	clipInfo.m_StartTime = cst->m_StartTime;
	clipInfo.m_StopTime = cst->m_StopTime;
	clipInfo.m_OrientationOffsetY = cst->m_OrientationOffsetY;
	clipInfo.m_Level = cst->m_Level;
	clipInfo.m_CycleOffset = cst->m_CycleOffset;
	clipInfo.m_LoopTime = cst->m_LoopTime;
	clipInfo.m_LoopBlend = cst->m_LoopBlend;
	clipInfo.m_LoopBlendOrientation = cst->m_LoopBlendOrientation;
	clipInfo.m_LoopBlendPositionY = cst->m_LoopBlendPositionY;
	clipInfo.m_LoopBlendPositionXZ = cst->m_LoopBlendPositionXZ;
	clipInfo.m_KeepOriginalOrientation = cst->m_KeepOriginalOrientation;
	clipInfo.m_KeepOriginalPositionY = cst->m_KeepOriginalPositionY;
	clipInfo.m_KeepOriginalPositionXZ = cst->m_KeepOriginalPositionXZ;
	clipInfo.m_HeightFromFeet = cst->m_HeightFromFeet;
	clipInfo.m_Mirror = cst->m_Mirror;
}

///@TODO: On the runs.fbx there are a ton of curves. Check up on what is going on...

template<class T>
static ClipOptType ClassifyCurve (AnimationCurveTpl<T>& curve, bool useHighQualityCurve)
{
	if (curve.GetKeyCount() == 0)
		return kInvalidCurve;

	if (IsConstantCurve(curve))
		return kConstantClip;

	if (!useHighQualityCurve && IsDenseCurve(curve))
		return kDenseClip;

	return kStreamedClip;
}

template<class T>
static void AddCurveToConstantClip (mecanim::animation::ConstantClip& clip, int index, AnimationCurveTpl<T>& curve)
{
    memcpy(&clip.data[index], &curve.GetKey(0).value, sizeof(T));
}

static void AddMappedPPtrCurveToStreamedClip (StreamedClipBuilder* builder, int curveIter, UnityEngine::Animation::AnimationClipBindingConstant& clipBindings, const PPtrKeyframes& pptrCurve)
{
	const size_t keyframeCount = pptrCurve.size();
	
	float* inTime;
	int* inValue;
	ALLOC_TEMP(inTime, float, keyframeCount);
	ALLOC_TEMP(inValue, int, keyframeCount);
	
	const int mapOffset = clipBindings.pptrCurveMapping.size();
	for (int i = 0; i < keyframeCount; ++i)
	{
		inTime[i] = pptrCurve[i].time;
		// Map Object to index
		inValue[i] = mapOffset + i;
		clipBindings.pptrCurveMapping.push_back(pptrCurve[i].value);
	}
	
	AddIntegerCurveToStreamedClip(builder, curveIter, inTime, inValue, keyframeCount);
}

template<typename TYPE> void for_each_curve(MecanimClipBuilder& clipBuilder, TYPE const& curves )
{
	for (int i=0;i<curves.size();i++)
	{
		std::pair<float, float> range = curves[i]->GetRange ();
		clipBuilder.startTime = std::min(range.first, clipBuilder.startTime);
		clipBuilder.stopTime = std::max(range.second, clipBuilder.stopTime);
	}
}

void ComputeDenseClipRange(MecanimClipBuilder& clipBuilder)
{
	MecanimClipBuilder::Curves& curves = clipBuilder.curves[kDenseClip];
	for_each_curve(clipBuilder, curves.positionCurves);
	for_each_curve(clipBuilder, curves.rotationCurves);
	for_each_curve(clipBuilder, curves.scaleCurves);
	for_each_curve(clipBuilder, curves.genericCurves);
		
	for (int i=0;i<curves.pptrCurves.size();i++)
	{
		PPtrKeyframes& keyFrames = *curves.pptrCurves[i];
		for(int j=0;j<keyFrames.size();j++)
		{
			clipBuilder.startTime = std::min(keyFrames[j].time, clipBuilder.startTime);
			clipBuilder.stopTime = std::max(keyFrames[j].time, clipBuilder.stopTime);
		}
	}

	clipBuilder.startTime = !IsFinite(clipBuilder.startTime) ? 0.0f : clipBuilder.startTime;
	clipBuilder.stopTime = !IsFinite(clipBuilder.stopTime) ? 0.0f : clipBuilder.stopTime;
}

bool PrepareClipBuilder (MecanimClipBuilder& clipBuilder)
{
	size_t previousTypesCurveCount = 0;

	ComputeDenseClipRange(clipBuilder);

	for (int t=0;t<kClipOptCount;t++)
	{
		MecanimClipBuilder::Curves& curves = clipBuilder.curves[t];
		
		size_t keyCount = 0;
		size_t genericBindingIndex = 0;
		size_t curveCount = 0;
		for (int i=0;i<curves.positionCurves.size();i++)
		{
			keyCount += curves.positionCurves[i]->GetKeyCount() * 3;
            curveCount += 3;
            genericBindingIndex++;
        }
		
		for (int i=0;i<curves.rotationCurves.size();i++)
		{
			keyCount += curves.rotationCurves[i]->GetKeyCount() * 4;
			curveCount += 4;
			genericBindingIndex++;
		}
		
		for (int i=0;i<curves.scaleCurves.size();i++)
		{
			keyCount += curves.scaleCurves[i]->GetKeyCount() * 3;
			curveCount += 3;
			genericBindingIndex++;
		}
		
		for (int i=0;i<curves.genericCurves.size();i++)
		{
			if (IsMuscleBinding(curves.bindings[genericBindingIndex]))
				clipBuilder.muscleIndexArray[curves.bindings[genericBindingIndex].attribute] = curveCount + previousTypesCurveCount;

			keyCount += curves.genericCurves[i]->GetKeyCount();
			genericBindingIndex++;
			curveCount++;
		}
		
		for (int i=0;i<curves.pptrCurves.size();i++)
		{
			keyCount += curves.pptrCurves[i]->size();
			curveCount++;
		}
		
		curves.totalKeyCount = keyCount;
        curves.totalCurveCount = curveCount;
		previousTypesCurveCount += curveCount;
	}
	
	clipBuilder.totalCurveCount = 0;
	clipBuilder.totalBindingCount = 0;
	for (int t=0;t<kClipOptCount;t++)
	{
		MecanimClipBuilder::Curves& curves = clipBuilder.curves[t];
		clipBuilder.totalBindingCount += curves.bindings.size();
		clipBuilder.totalCurveCount += curves.totalCurveCount;
	}

	return clipBuilder.totalCurveCount != 0 || clipBuilder.hasAnimationEvents;
}

mecanim::animation::ClipMuscleConstant* BuildMuscleClip (const MecanimClipBuilder& clipBuilder, const AnimationClipSettings& animationClipSettings, bool isHumanClip, UnityEngine::Animation::AnimationClipBindingConstant& outClipBindings, mecanim::memory::Allocator& allocator)
{
	SETPROFILERLABEL(ClipMuscleConstant);

    // Total binding count
    outClipBindings.genericBindings.clear();
    outClipBindings.genericBindings.reserve(clipBuilder.totalBindingCount);
    outClipBindings.pptrCurveMapping.clear();
	
	// Combine into a single binding array
	outClipBindings.genericBindings.reserve(clipBuilder.totalBindingCount);
	for (int i=0;i<kClipOptCount;i++)
		outClipBindings.genericBindings.insert(outClipBindings.genericBindings.end(), clipBuilder.curves[i].bindings.begin(), clipBuilder.curves[i].bindings.end());
	
    mecanim::animation::Clip* clip = mecanim::animation::CreateClipSimple (clipBuilder.totalCurveCount, allocator);
	
    // Streamed clip
	const MecanimClipBuilder::Curves& streamedCurves = clipBuilder.curves[kStreamedClip];
    StreamedClipBuilder* builder = NULL;
    builder = CreateStreamedClipBuilder(streamedCurves.totalCurveCount, streamedCurves.totalKeyCount);
	
    // Constant clip
    const MecanimClipBuilder::Curves& constantCurves = clipBuilder.curves[kConstantClip];
    CreateConstantClip (clip->m_ConstantClip, constantCurves.totalCurveCount, allocator);

	 // Dense clip
    const MecanimClipBuilder::Curves& denseCurves = clipBuilder.curves[kDenseClip];
	CreateDenseClip (clip->m_DenseClip, denseCurves.totalCurveCount, clipBuilder.startTime, clipBuilder.stopTime, clipBuilder.sampleRate, allocator);
	
    for (int t=0;t<kClipOptCount;t++)
    {
		const MecanimClipBuilder::Curves& curves = clipBuilder.curves[t];
		
		#define AddCurveByType(CURVE_TYPE) \
			if (t == kStreamedClip) \
				AddCurveToStreamedClip(builder, curveIter, *curves.CURVE_TYPE[i]); \
			else if (t == kDenseClip) \
				AddCurveToDenseClip(clip->m_DenseClip, curveIter, *curves.CURVE_TYPE[i]); \
			else if (t == kConstantClip) \
				AddCurveToConstantClip (clip->m_ConstantClip, curveIter, *curves.CURVE_TYPE[i]);
			
		size_t curveIter = 0;
        for (int i=0;i<curves.positionCurves.size();i++, curveIter+=3)
        {
			AddCurveByType(positionCurves)
        }
		
        for (int i=0;i<curves.rotationCurves.size();i++, curveIter+=4)
		{
			AddCurveByType(rotationCurves)
		}
		
        for (int i=0;i<curves.scaleCurves.size();i++,curveIter+=3)
		{
			AddCurveByType(scaleCurves)
		}
		
        for (int i=0;i<curves.genericCurves.size();i++,curveIter++)
		{
			AddCurveByType(genericCurves)
		}
		
        for (int i=0;i<curves.pptrCurves.size();i++,curveIter++)
		{
			Assert(t == kStreamedClip);
			AddMappedPPtrCurveToStreamedClip(builder, curveIter, outClipBindings, *streamedCurves.pptrCurves[i]);
		}
    }
	
	if (builder)
	{
		CreateStreamClipConstant(builder, clip->m_StreamedClip, allocator);
		DestroyStreamedClipBuilder(builder);
	}
	
	mecanim::animation::ClipMuscleConstant* muscleClip = mecanim::animation::CreateClipMuscleConstant(clip, allocator);
	for(mecanim::uint32_t muscleIter = 0; muscleIter < mecanim::animation::s_ClipMuscleCurveCount; muscleIter++)
		muscleClip->m_IndexArray[muscleIter] = clipBuilder.muscleIndexArray[muscleIter];
	
	PatchMuscleClipWithInfo (animationClipSettings, isHumanClip, muscleClip);
	
	return muscleClip;
}

void AddPositionCurveToClipBuilder (AnimationCurveVec3& curve, const UnityStr& path, MecanimClipBuilder& clipBuilder, bool useHighQualityCurve)
{
	ClipOptType type = ClassifyCurve (curve, useHighQualityCurve);
	if (type == kInvalidCurve)
		return;
	
	MecanimClipBuilder::Curves& curves = clipBuilder.curves[type];
	curves.positionCurves.push_back(&curve);
	CreateTransformBinding (path, UnityEngine::Animation::kBindTransformPosition, curves.bindings.push_back());
}

void AddRotationCurveToClipBuilder (AnimationCurveQuat& curve, const UnityStr& path, MecanimClipBuilder& clipBuilder, bool useHighQualityCurve)
{
	ClipOptType type = ClassifyCurve (curve, useHighQualityCurve);
	if (type == kInvalidCurve)
		return;
	
    MecanimClipBuilder::Curves& curves = clipBuilder.curves[type];
	curves.rotationCurves.push_back(&curve);
	CreateTransformBinding (path, UnityEngine::Animation::kBindTransformRotation, curves.bindings.push_back());
}

void AddScaleCurveToClipBuilder (AnimationCurveVec3& curve, const UnityStr& path, MecanimClipBuilder& clipBuilder, bool useHighQualityCurve)
{
	ClipOptType type = ClassifyCurve (curve, useHighQualityCurve);
	if (type == kInvalidCurve)
		return;
	
	MecanimClipBuilder::Curves& curves = clipBuilder.curves[type];
	curves.scaleCurves.push_back(&curve);
	CreateTransformBinding (path, UnityEngine::Animation::kBindTransformScale, curves.bindings.push_back());
}

void AddGenericCurveToClipBuilder (AnimationCurve& curve, const UnityEngine::Animation::GenericBinding& binding, MecanimClipBuilder& clipBuilder, bool useHighQualityCurve)
{
	ClipOptType type = ClassifyCurve (curve, useHighQualityCurve);
	if (type == kInvalidCurve)
		return;
	
	MecanimClipBuilder::Curves& curves = clipBuilder.curves[type];
	curves.genericCurves.push_back(&curve);
	curves.bindings.push_back(binding);
}

void AddPPtrCurveToClipBuilder (PPtrKeyframes& curve, const UnityEngine::Animation::GenericBinding& binding, MecanimClipBuilder& clipBuilder)
{
	if (curve.empty())
		return;
	
	// The runtime binding code is not able to handle when a Transform component incorrectly has pptr curve.
	// Reject it here..
	if (binding.classID == ClassID(Transform))
		return;
	
	MecanimClipBuilder::Curves& curves = clipBuilder.curves[kStreamedClip];
	curves.pptrCurves.push_back(&curve);
	curves.bindings.push_back(binding);
}
