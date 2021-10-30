#include "UnityPrefix.h"
#include "KeyframeReducer.h"
#include "AnimationClip.h"
#include "Runtime/Math/Quaternion.h"
#include "AnimationCurveUtility.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Animation/MecanimClipBuilder.h"
#include <sstream>
#include <vector>

using namespace std;

#define DEBUG_COMPRESSION 0

const float kPositionMinValue = 0.00001F;
const float kQuaternionNormalizationError = 0.001F;/// The sampled quaternion must be almost normalized.

/// - We allow reduction if the reduced magnitude doesn't go off very far
/// - And the angle between the two rotations is similar
inline bool QuaternionDistanceError (Quaternionf value, Quaternionf reduced, const float quaternionDotError)
{	
	float magnitude = Magnitude(reduced);
	if (!CompareApproximately(1.0F, magnitude, kQuaternionNormalizationError))
	{
		return false;
	}

	value = NormalizeSafe(value);
	reduced = reduced / magnitude;
	
	// float angle = Rad2Deg(acos (Dot(value, reduced))) * 2.0F;
	// if (dot > kQuaternionAngleError)
	//  	return false;
	if (Dot(value, reduced) < quaternionDotError)
	{
		return false;
	}

	return true;
}

bool DeltaError(const float value, const float reducedValue, const float delta, const float percentage, const float minValue)
{
	const float absValue = Abs(value);
	// (absValue > minValue || Abs(reducedValue) > minValue) part is necessary for reducing values which have tiny fluctuations around 0
	return (absValue > minValue || Abs(reducedValue) > minValue) && (delta > absValue * percentage);
}

/// We allow reduction 
// - the distance of the two vectors is low
// - the distance of each axis is low
inline bool PositionDistanceError (Vector3f value, Vector3f reduced, const float distancePercentageError)
{
	const float percentage = distancePercentageError;
	const float minValue = kPositionMinValue * percentage;

	// Vector3 distance as a percentage
	float distance = SqrMagnitude(value - reduced);
	float length = SqrMagnitude(value);
	float lengthReduced = SqrMagnitude(reduced);
	//if (distance > length * Sqr(percentage))
	if (DeltaError(length, lengthReduced, distance, Sqr(percentage), Sqr(minValue)))
		return false;

	// Distance of each axis
	float distanceX = Abs(value.x - reduced.x);
	float distanceY = Abs(value.y - reduced.y);
	float distanceZ = Abs(value.z - reduced.z);
	
	//if (distanceX > Abs(value.x) * percentage)
	if (DeltaError(value.x, reduced.x, distanceX, percentage, minValue))
		return false;
	//if (distanceY > Abs(value.y) * percentage)
	if (DeltaError(value.y, reduced.y, distanceY, percentage, minValue))
		return false;
	//if (distanceZ > Abs(value.z) * percentage)
	if (DeltaError(value.z, reduced.z, distanceZ, percentage, minValue))
		return false;

	return true;
}

/// We allow reduction if the distance between the two values is low
inline bool FloatDistanceError (float value, float reduced, const float distancePercentageError)
{
	const float percentage = distancePercentageError;
	const float minValue = kPositionMinValue * percentage;

	float distance = Abs(value - reduced);
	//if (distance > Abs(value) * percentage)
	if (DeltaError(value, reduced, distance, percentage, minValue))
		return false;
	
	return true;
}

// Checks if reduced curve is valid at time "time"
template<class T, class ErrorFunction>
bool CanReduce(AnimationCurveTpl<T>& curve, const KeyframeTpl<T>& key0, const KeyframeTpl<T>& key1, float time, ErrorFunction canReduceFunction, const float allowedError)
{
	T value = curve.Evaluate(time);
	T reduced_value = InterpolateKeyframe(key0, key1, time);

	return canReduceFunction(value, reduced_value, allowedError);
}

/*template<class T>
void FitTangentsToCurve(AnimationCurveTpl<T>& curve, KeyframeTpl<T>& key0, KeyframeTpl<T>& key1)
{
	// perform curve fitting

	const float t0 = key0.time;
	const float dt = key1.time - key0.time;

	float time1 = 0.3f;
	float time2 = 1 - time1;

	// points on the curve at time1 and time2
	const T v0 = curve.Evaluate(t0 + time1 * dt);
	const T v1 = curve.Evaluate(t0 + time2 * dt);

	FitTangents(key0, key1, time1, time2, v0, v1);
}*/

template<class T, class ErrorFunction>
bool CanReduce(const KeyframeTpl<T>& fromKey, const KeyframeTpl<T>& toKey, AnimationCurveTpl<T>& curve, ErrorFunction canReduceFunction, const float allowedError, const float delta,
	int firstKey, int lastKey, bool useKeyframeLimit)
{
	const float beginTime = fromKey.time;
	const float endTime = toKey.time;

	// We simply sample every frame and compare the original curve against, if we simply removed one key.
	// If the error between the curves is not too big, we just remove the key
	bool canReduce = true;

	for (float t = beginTime + delta; t < endTime; t += delta)
	{
		if (!(canReduce = CanReduce(curve, fromKey, toKey, t, canReduceFunction, allowedError)))
			break;
	}

	// we need to check that all keys can be reduced, because keys might be closer to each other than delta
	// this happens when we have steps in curve
	// TODO : we could skip the loop above if keyframes are close enough

	float lastTime = beginTime;

	for (int j = firstKey; canReduce && j < lastKey; ++j)
	{			
		const float time = curve.GetKey(j).time;

		// validates point at keyframe (j) and point between (j) and and (j-1) keyframes
		// TODO : For checking point at "time" it could just use keys[j].value instead - that would be faster than sampling the curve
		canReduce = 
			CanReduce(curve, fromKey, toKey, time, canReduceFunction, allowedError) &&
			CanReduce(curve, fromKey, toKey, (lastTime + time) / 2, canReduceFunction, allowedError);

		lastTime = time;
	}

	if (canReduce)
	{
		// validate point between last two keyframes
		float time = curve.GetKey(lastKey).time;
		canReduce = CanReduce(curve, fromKey, toKey, (lastTime + time) / 2, canReduceFunction, allowedError);
	}

	// Don't reduce if we are about to reduce more than 50 samples at 
	// once to prevent n^2 performance impact
	canReduce = canReduce && (!useKeyframeLimit || (endTime - beginTime < 50.0F * delta));

	return canReduce;
}

template<class T, class ErrorFunction>
float ReduceKeyframes (AnimationCurveTpl<T>& curve, float sampleRate, ErrorFunction canReduceFunction, const float allowedError, T zeroValue, bool optimalCurveRepresentation)
{
	AssertMsg(curve.GetKeyCount() >= 2, "Key count: %d", curve.GetKeyCount());

	if (curve.GetKeyCount() <= 2)
		return 100.0F;

	dynamic_array<typename AnimationCurveTpl<T>::Keyframe> output;
	output.reserve(curve.GetKeyCount());

	float delta = 1.f / sampleRate;

	// at first try to reduce to const curve
	typename AnimationCurveTpl<T>::Keyframe firstKey = curve.GetKey(0);
	typename AnimationCurveTpl<T>::Keyframe lastKey = curve.GetKey(curve.GetKeyCount() - 1);
	firstKey.inSlope = firstKey.outSlope = zeroValue;
	lastKey.inSlope = lastKey.outSlope = zeroValue;
	lastKey.value = firstKey.value;	

	const bool canReduceToConstCurve = CanReduce(firstKey, lastKey, curve, canReduceFunction, allowedError, delta, 0, curve.GetKeyCount() - 1, false);		
	if (canReduceToConstCurve)
	{
		output.reserve(2);
		output.push_back(firstKey);
		output.push_back(lastKey);
	}
	else
	{	
		output.reserve(curve.GetKeyCount());
		// We always add the first key
		output.push_back(curve.GetKey(0));
		
		int lastUsedKey = 0;
		
		for (int i=1;i<curve.GetKeyCount() - 1;i++)
		{
			typename AnimationCurveTpl<T>::Keyframe fromKey = curve.GetKey(lastUsedKey);
			typename AnimationCurveTpl<T>::Keyframe toKey = curve.GetKey(i + 1);

			//FitTangentsToCurve(curve, fromKey, toKey);

			const bool canReduce = CanReduce(fromKey, toKey, curve, canReduceFunction, allowedError, delta, lastUsedKey + 1, i + 1, true);

			if (!canReduce)
			{
				output.push_back(curve.GetKey(i));
				// fitting tangents between last two keys
				//FitTangentsToCurve(curve, *(output.end() - 2), output.back());

				lastUsedKey = i;
			}
		}
		
		// We always add the last key
		output.push_back(curve.GetKey(curve.GetKeyCount() - 1));
		// fitting tangents between last and the one before last keys
		//FitTangentsToCurve(curve, *(output.end() - 2), output.back());		
	}

	float reduction = (float)output.size() / (float)curve.GetKeyCount() * 100.0F;

	curve.Swap(output);

	// if we want optimal curve representation and reduced curve can be represeted with a dense curve 
	// keep original curve for better quality sampling 
	if (optimalCurveRepresentation && IsDenseCurve(curve))
	{
		curve.Swap(output);
		reduction = 100.0F;
	}

	return reduction;
}

template <class T, class U, typename ReductionFunction>
float ReduceKeyframes (const float sampleRate, T& curves, ReductionFunction reductionFunction, const float allowedError, const U zeroValue, bool optimalCurveRepresentation)
{
	float totalRatio = 0;
	for (typename T::iterator it = curves.begin(), end = curves.end(); it != end; ++it)
	{
		float compressionRatio = ReduceKeyframes(it->curve, sampleRate, reductionFunction, allowedError, zeroValue, optimalCurveRepresentation);
		#if DEBUG_COMPRESSION
			printf_console ("Compression %f%% of rotation %s\n", compressionRatio, i->path.c_str());
		#endif

		totalRatio += compressionRatio;
	}

	return totalRatio;
}

void ReduceKeyframes (AnimationClip& clip, float rotationError, float positionError, float scaleError, float floatError)
{
	AnimationClip::QuaternionCurves& rot = clip.GetRotationCurves();
	AnimationClip::Vector3Curves& pos = clip.GetPositionCurves();
	AnimationClip::Vector3Curves& scale = clip.GetScaleCurves();
	AnimationClip::FloatCurves& floats = clip.GetFloatCurves();
	AnimationClip::FloatCurves& editorCurves = clip.GetEditorCurvesNoConversion();
	
	rotationError = cos(Deg2Rad(rotationError) / 2.0F);
	positionError = positionError / 100.0F;
	scaleError = scaleError / 100.0F;
	floatError = floatError / 100.0F;

	float time = GetTimeSinceStartup();
	float averageCompressionRatio = 0;	

	const float sampleRate = clip.GetSampleRate();

	averageCompressionRatio += ReduceKeyframes (sampleRate, rot, QuaternionDistanceError, rotationError, Quaternionf(0, 0, 0, 0), clip.IsAnimatorMotion() && !clip.GetUseHighQualityCurve() );
	averageCompressionRatio += ReduceKeyframes (sampleRate, pos, PositionDistanceError, positionError, Vector3f::zero, clip.IsAnimatorMotion() && !clip.GetUseHighQualityCurve());
	averageCompressionRatio += ReduceKeyframes (sampleRate, scale, PositionDistanceError, scaleError, Vector3f::zero, clip.IsAnimatorMotion() && !clip.GetUseHighQualityCurve());
	averageCompressionRatio += ReduceKeyframes (sampleRate, floats, FloatDistanceError, floatError, 0.f, clip.IsAnimatorMotion() && !clip.GetUseHighQualityCurve());
	averageCompressionRatio += ReduceKeyframes (sampleRate, editorCurves, FloatDistanceError, floatError, 0.f, clip.IsAnimatorMotion() && !clip.GetUseHighQualityCurve());

	// If all the curves are empty, we end up with zero reduction.
	if (averageCompressionRatio == 0)
		return;

	averageCompressionRatio /= (rot.size() + pos.size() + scale.size() + floats.size() + editorCurves.size());
	time = GetTimeSinceStartup() - time;

	{
		std::ostringstream oss;
		oss << "Keyframe reduction: Ratio: " << averageCompressionRatio << "%; Time: " << time << "s;\n";
		printf_console("%s", oss.str().c_str());
	}
}

// TODO : use tangents from editor or perform curve fitting
// TODO : these should be removed eventually and custom settings should be used instead
const float kQuaternionAngleError = 0.5F;// The maximum angle deviation allowed in degrees
const float kQuaternionDotError = cos(Deg2Rad(kQuaternionAngleError) / 2.0F);
///@TODO: * Support step curves
///       * Improve keyframe reduction
///       * Make keyframe reduction faster
void EulerToQuaternionCurveBake (const AnimationCurve& curveX, const AnimationCurve& curveY, const AnimationCurve& curveZ, AnimationCurveQuat& collapsed, float sampleRate)
{
	float begin = std::numeric_limits<float>::infinity ();
	float end = -std::numeric_limits<float>::infinity ();

	float delta = 1.0F / sampleRate;	

	const AnimationCurve* curves[3] = { &curveX, &curveY, &curveZ };

	for (int i=0;i<3;i++)
	{
		if (curves[i]->GetKeyCount() >= 1)
		{
			begin = min(curves[i]->GetKey(0).time, begin);
			end = max(curves[i]->GetKey(curves[i]->GetKeyCount()-1).time, end);
		}
	}
	if (!IsFinite(begin) || !IsFinite(end))
		return;
	
	
	float deg2rad = Deg2Rad(1.0F);
	for (float i=begin;i < end + delta;i += delta)
	{
		if (i + delta / 2.0F > end)
			i = end;

		Vector3f euler = Vector3f (curveX.Evaluate(i), curveY.Evaluate(i), curveZ.Evaluate(i)) * deg2rad;
		Quaternionf q = EulerToQuaternion(euler);
		//Vector3f eulerBack = QuaternionToEuler (q) * Rad2Deg(1.0F);
		//printf_console("%f : %f, %f, %f ----- %f, %f, %f\n", i, euler.x, euler.y, euler.z, eulerBack.x, eulerBack.y, eulerBack.z);
	
		KeyframeTpl<Quaternionf> key;
		key.time = i;
		key.value = q;
		key.inSlope = key.outSlope = Quaternionf(0,0,0,0);
		collapsed.AddKeyBackFast(key);
		
		if (i == end)
			break;
	}
	
	EnsureQuaternionContinuityAndRecalculateSlope (collapsed);
	
	//@TODO: Keyframe reduction is disabled for now, with keyframe reduction enabled iteration time
	// in the animation window becomes unbearable
	// ReduceKeyframes(collapsed, sampleRate, QuaternionDistanceError, kQuaternionDotError, Quaternionf(0, 0, 0, 0), false);
}
