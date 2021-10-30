#include "UnityPrefix.h"
#include "AnimationClipUtility.h"
#include "AnimationClip.h"
#include "AnimationCurveUtility.h"

template <class T>
void EnsureLoopFrameContinuity (AnimationCurveTpl<T>& curve) {}

template <>
void EnsureLoopFrameContinuity<Quaternionf> (AnimationCurveTpl<Quaternionf>& curve) 
{
	EnsureQuaternionContinuityLoopFrame(curve);
}

template <class U, class T, typename A>
void ClipAnimations (const std::vector<T, A>& curves, float startTime, float endTime, float sampleRate, bool duplicateLastFrame, std::vector<T, A>& destinationCurves)
{
	for (typename std::vector<T, A>::const_iterator it = curves.begin(); it != curves.end(); ++it)
	{
		T newCurve;
		AssertMsg(it->curve.GetKeyCount() >= 2, "Key count: %d on curve %s", it->curve.GetKeyCount(), it->path.c_str());

		if (ClipAnimationCurve (it->curve, newCurve.curve, startTime, endTime))
		{
			it->CopyWithoutCurve(newCurve);

			newCurve.curve.SetPostInfinity(kClamp);
			newCurve.curve.SetPreInfinity(kClamp);
			
			if (duplicateLastFrame)
			{
				AddLoopingFrame(newCurve.curve, endTime - startTime + 1.0f/sampleRate);
				EnsureLoopFrameContinuity(newCurve.curve);
			}
			
			AssertMsg(newCurve.curve.GetKeyCount() >= 2, "Key count: %d on curve %s", newCurve.curve.GetKeyCount(), it->path.c_str());
			
			destinationCurves.push_back(newCurve);
		}
	}
}

void ClipAnimation (AnimationClip& sourceClip, AnimationClip& destinationClip, float startTimeSeconds, float endTimeSeconds, bool duplicateLastFrame)
{
	if (startTimeSeconds > endTimeSeconds)
		std::swap(endTimeSeconds, startTimeSeconds);
	
	ClipAnimations<Quaternionf>(sourceClip.GetRotationCurves(), startTimeSeconds, endTimeSeconds, sourceClip.GetSampleRate(), duplicateLastFrame, destinationClip.GetRotationCurves());
	ClipAnimations<Vector3f>(sourceClip.GetPositionCurves(), startTimeSeconds, endTimeSeconds, sourceClip.GetSampleRate(), duplicateLastFrame, destinationClip.GetPositionCurves());
	ClipAnimations<Vector3f>(sourceClip.GetScaleCurves(), startTimeSeconds, endTimeSeconds, sourceClip.GetSampleRate(), duplicateLastFrame, destinationClip.GetScaleCurves());
	ClipAnimations<float>(sourceClip.GetFloatCurves(), startTimeSeconds, endTimeSeconds, sourceClip.GetSampleRate(), duplicateLastFrame, destinationClip.GetFloatCurves());
#if UNITY_EDITOR
	ClipAnimations<float>(sourceClip.GetEditorCurvesNoConversion(), startTimeSeconds, endTimeSeconds, sourceClip.GetSampleRate(), duplicateLastFrame, destinationClip.GetEditorCurvesNoConversion());
	AssertIf(sourceClip.GetEditorCurvesNoConversion().size() < destinationClip.GetEditorCurvesNoConversion().size());
#endif // #if UNITY_EDITOR

	AssertIf(sourceClip.GetRotationCurves().size() < destinationClip.GetRotationCurves().size());
	AssertIf(sourceClip.GetPositionCurves().size() < destinationClip.GetPositionCurves().size());
	AssertIf(sourceClip.GetScaleCurves().size() < destinationClip.GetScaleCurves().size());
	AssertIf(sourceClip.GetFloatCurves().size() < destinationClip.GetFloatCurves().size());
}


void CopyAnimation (AnimationClip& sourceClip, AnimationClip& destinationClip)
{
	destinationClip.GetRotationCurves() = sourceClip.GetRotationCurves();
	destinationClip.GetPositionCurves() = sourceClip.GetPositionCurves();
	destinationClip.GetScaleCurves() = sourceClip.GetScaleCurves();
	destinationClip.GetFloatCurves() = sourceClip.GetFloatCurves();
#if UNITY_EDITOR
	destinationClip.GetEditorCurvesNoConversion() = sourceClip.GetEditorCurvesNoConversion();
#endif // #if UNITY_EDITOR
}
