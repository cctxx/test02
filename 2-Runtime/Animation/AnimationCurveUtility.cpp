#include "UnityPrefix.h"
#include "AnimationCurveUtility.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Utilities/Utility.h"

template<class T>
T SafeDeltaDivide (T y, float x)
{
	if (Abs(x) > kCurveTimeEpsilon)
		return y / x;
	else
		return Zero<T>();
}




template<class T>
inline T HermiteInterpolateDerived (float t, T p0, T m0, T m1, T p1)
{
	float t2 = t * t;
	
	float a =  6.0F * t2 - 6.0F * t;
	float b =  3.0F * t2 - 4.0F * t + 1.0F;
	float c =  3.0F * t2 - 2.0F * t;
	float d = -6.0F * t2 + 6.0F * t;
	
	return a * p0 + b * m0 + c * m1 + d * p1;
}

template<class T>
void RecalculateSplineSlopeT(AnimationCurveTpl<T>& curve, int key, float bias = 0.0F);

using namespace std;

// TODO : maybe we can remove it?
// this function is used by ImportFBX
void EnsureQuaternionContinuity (AnimationCurve** curves)
{
	if (!curves[0] || !curves[1] || !curves[2] || !curves[3])
		return;
		
	int keyCount = curves[0]->GetKeyCount ();
	if (keyCount != curves[1]->GetKeyCount () || keyCount != curves[2]->GetKeyCount () || keyCount != curves[3]->GetKeyCount ())
		return;
	
	if (keyCount == 0)
		return;
	
	Quaternionf last (curves[0]->GetKey (keyCount-1).value, curves[1]->GetKey (keyCount-1).value, curves[2]->GetKey (keyCount-1).value, curves[3]->GetKey (keyCount-1).value);
	for (int i=0;i<keyCount;i++)
	{
		Quaternionf cur (curves[0]->GetKey (i).value, curves[1]->GetKey (i).value, curves[2]->GetKey (i).value, curves[3]->GetKey (i).value);
		if (Dot (cur, last) < 0.0F)
			cur = Quaternionf (-cur.x, -cur.y, -cur.z, -cur.w);
		last = cur;
		curves[0]->GetKey (i).value = cur.x;
		curves[1]->GetKey (i).value = cur.y;
		curves[2]->GetKey (i).value = cur.z;
		curves[3]->GetKey (i).value = cur.w;
	}
	
	for (int j=0;j<4;j++)
	{
		for (int i=0;i<keyCount;i++)
			RecalculateSplineSlopeT (*curves[j], i);
	}
}


void ExpandQuaternionCurve (AnimationCurveQuat& quat, AnimationCurve* outCurves[4])
{
	int size = quat.GetKeyCount();
	
	for (int c=0;c<4;c++)
		outCurves[c]->ResizeUninitialized(size);
	
	for (int i=0;i<size;i++)
	{
		AnimationCurve::Keyframe key;
		const AnimationCurveQuat::Keyframe& src = quat.GetKey(i);
		key.time = src.time;
		for (int c=0;c<4;c++)
		{
			key.value = src.value[c];
			key.inSlope = src.inSlope[c];
			key.outSlope = src.outSlope[c];
			outCurves[c]->GetKey(i) = key;
		}
	}

	for (int c=0;c<4;c++)
	{
		outCurves[c]->SetPreInfinity(quat.GetPreInfinity());
		outCurves[c]->SetPostInfinity(quat.GetPostInfinity());
		outCurves[c]->InvalidateCache();
	}
}

void ExpandVector3Curve (AnimationCurveVec3& inCurve, AnimationCurve* outCurves[3])
{
	int size = inCurve.GetKeyCount();
	
	for (int c=0;c<3;c++)
		outCurves[c]->ResizeUninitialized(size);
	
	for (int i=0;i<size;i++)
	{
		AnimationCurve::Keyframe key;
		const AnimationCurveVec3::Keyframe& src = inCurve.GetKey(i);
		key.time = src.time;
		for (int c=0;c<3;c++)
		{
			key.value = src.value[c];
			key.inSlope = src.inSlope[c];
			key.outSlope = src.outSlope[c];
			outCurves[c]->GetKey (i) = key;
		}
	}
	
	for (int c=0;c<3;c++)
	{
		outCurves[c]->SetPreInfinity(inCurve.GetPreInfinity());
		outCurves[c]->SetPostInfinity(inCurve.GetPostInfinity());
		outCurves[c]->InvalidateCache();
	}
}

template<class T>
int AddInbetweenKey (AnimationCurveTpl<T>& curve, float curveT)
{
	int index = curve.FindIndex (curveT);
	if (index == -1)
		return -1;
	const KeyframeTpl<T>& lhs = curve.GetKey (index);
	const KeyframeTpl<T>& rhs = curve.GetKey (min(index+1, curve.GetKeyCount()-1));
		
	return curve.AddKey(CalculateInbetweenKey(lhs, rhs, curveT));	
}


template<class T>
KeyframeTpl<T> CalculateInbetweenKey(const AnimationCurveTpl<T>& curve, float curveT)
{
	int index = curve.FindIndex (curveT);
	const typename AnimationCurveTpl<T>::Keyframe& lhs = curve.GetKey (index);
	const typename AnimationCurveTpl<T>::Keyframe& rhs = curve.GetKey (index+1);
	return CalculateInbetweenKey(lhs, rhs, curveT);	
}


template<class T>
KeyframeTpl<T> CalculateInbetweenKey(const KeyframeTpl<T>& lhs, const KeyframeTpl<T>& rhs, float curveT)
{
	typename AnimationCurveTpl<T>::Keyframe key;
	float dx = rhs.time - lhs.time;
	
	AssertIf(dx == 0.0F);
	
	float t = (curveT - lhs.time) / dx;
	
	if (t < -kCurveTimeEpsilon)
	{
		key = lhs;
		key.time = curveT;
		key.inSlope = Zero<T>();
		key.outSlope = Zero<T>();
		
		return key;
	}
	else if (t > 1.0F + kCurveTimeEpsilon)
	{
		key = rhs;
		key.time = curveT;
		key.inSlope = Zero<T>();
		key.outSlope = Zero<T>();
		return key;
	}
	
	T m1 = lhs.outSlope * dx;
	T m2 = rhs.inSlope * dx;
	
	// Calculate the slope at t. This is simply done by deriving the Hermite basis functions
	// and feeding the derived hermite interpolator normal curve values
	T slope = HermiteInterpolateDerived (t, lhs.value, m1, m2, rhs.value);
	if (dx > 1.0F / MaxTan<float>())
		slope /= dx;
	else
		slope = MaxTan<T> ();
	
	HandleSteppedTangent(lhs, rhs, slope);
	
	key.inSlope = slope;
	key.outSlope = slope;
	
	// the value of the key is just interpolated	
	key.time = curveT;
	key.value = HermiteInterpolate (t, lhs.value, m1, m2, rhs.value);

	HandleSteppedCurve(lhs, rhs, key.value);

	AssertIf(!IsFinite(key.value));
	
	return key;
}

void QuaternionCurveToEulerCurve (AnimationCurveQuat& quat, AnimationCurve* outCurves[3])
{
	int size = quat.GetKeyCount();
	
	for (int c=0;c<3;c++)
		outCurves[c]->ResizeUninitialized(size);
	
	for (int i=0;i<size;i++)
	{
		AnimationCurve::Keyframe key;
		const AnimationCurveQuat::Keyframe& src = quat.GetKey(i);
		key.time = src.time;

		float idt = i > 0 ? key.time - quat.GetKey(i-1).time : quat.GetKey(i+1).time - key.time;
		float odt = i < size-1 ? quat.GetKey(i+1).time - key.time : key.time - quat.GetKey(i-1).time;

		Quaternionf quat = src.value;
		Quaternionf iquat = quat + src.inSlope * idt / 3; 
		Quaternionf oquat = quat + src.outSlope * odt / 3; 

		quat = NormalizeSafe(quat);
		iquat = NormalizeSafe(iquat);
		oquat = NormalizeSafe(oquat);
		
		Vector3f euler = QuaternionToEuler(quat) * Rad2Deg(1.0F);
		Vector3f ieuler = QuaternionToEuler(iquat) * Rad2Deg(1.0F);
		Vector3f oeuler = QuaternionToEuler(oquat) * Rad2Deg(1.0F);
		
		for (int c=0;c<3;c++)
		{
			ieuler[c] = Repeat(ieuler[c] - euler[c] + 180.0F, 360.0F) + euler[c] - 180.0F;
			oeuler[c] = Repeat(oeuler[c] - euler[c] + 180.0F, 360.0F) + euler[c] - 180.0F;
			
			key.value = euler[c];
			key.inSlope = 3 * (ieuler[c] - euler[c]) / idt;
			key.outSlope = 3 * (oeuler[c] - euler[c]) / odt;
			outCurves[c]->GetKey (i) = key;
		}
	}

	for (int c=0;c<3;c++)
	{
		outCurves[c]->SetPreInfinity(quat.GetPreInfinity());
		outCurves[c]->SetPostInfinity(quat.GetPostInfinity());
		outCurves[c]->InvalidateCache();
	}
}

Quaternionf EvaluateQuaternionFromEulerCurves (const AnimationCurve& curveX, const AnimationCurve& curveY, const AnimationCurve& curveZ, float time)
{
	Vector3f euler;
	euler.x = curveX.Evaluate(time);
	euler.y = curveY.Evaluate(time);
	euler.z = curveZ.Evaluate(time);
	return EulerToQuaternion (euler * Deg2Rad(1.0F));
}

void EulerToQuaternionCurve (const AnimationCurve& curveX, const AnimationCurve& curveY, const AnimationCurve& curveZ, AnimationCurveQuat& collapsed)
{
	int size, foundIndex;

	float errorDelta = 0.002F;
	
	const AnimationCurve* curves[3] = { &curveX, &curveY, &curveZ };

	// Create keyframes in collapsed array with filled out time values based on keyframes of all 3 curves
	for (int c=0;c<3;c++)
	{
		const AnimationCurve& curve = *curves[c];
		size = curve.GetKeyCount();
		for (int i=0;i<size;i++)
		{
			const float srcTime = curve.GetKey(i).time;
			
			// Just add keyframe if there are not enough keys in curve
			///@TODO: incorrect when one curve has only one key, because it will just call AddKey multiple times on the same key potentially.
			/// Some other code further down is doing the same thing...
			bool addKey = true;
			
			if (collapsed.IsValid())
			{
				foundIndex = collapsed.FindIndex(srcTime);
				
				bool addKey = foundIndex < 0;
				
				if (!addKey)
				{				
					// Do we have a key that is in the curve but not in the collapsed curve?
					// We check keys on the left and on the right of the found key
					addKey = 
						!CompareApproximately(srcTime, collapsed.GetKey(foundIndex).time, errorDelta) && 
						(foundIndex + 1 < collapsed.GetKeyCount() || !CompareApproximately(srcTime, collapsed.GetKey(foundIndex + 1).time, errorDelta));
				}
			}
			
			if (addKey)
			{
				KeyframeTpl<Quaternionf> dst;
				dst.time = srcTime;
				collapsed.AddKey(dst);	
			}
		}
	}

	// Evaluate values at keys
	size = collapsed.GetKeyCount();
	for (int i=0; i<size; i++)
	{
		// This part would be semi incorrect if the euler curves didn't all have the same keyframes,
		// But luckily the UI enforces them to always be together.
		float time = collapsed.GetKey(i).time;
		KeyframeTpl<Quaternionf>& dst = collapsed.GetKey(i);
		dst.value = EvaluateQuaternionFromEulerCurves(curveX, curveY, curveZ, time);
	}
	
	// Determine tangents in quaternion space by sampling deltas
	// TODO: Use better way of sampling tangents
	for (int i=0; i<size-1; i++)
	{
		float lTime = collapsed.GetKey(i).time;
		float rTime = collapsed.GetKey(i+1).time;
		
		// Sample euler curves epsilon time efter left key and get quaternion
		Quaternionf quat = EvaluateQuaternionFromEulerCurves(curveX, curveY, curveZ, lTime*0.999F + rTime * 0.001F);
		KeyframeTpl<Quaternionf>& dst = collapsed.GetKey(i);
		Quaternionf qDelta ((quat.x - dst.value.x) * 1000/(rTime-lTime),
							(quat.y - dst.value.y) * 1000/(rTime-lTime),
							(quat.z - dst.value.z) * 1000/(rTime-lTime),
							(quat.w - dst.value.w) * 1000/(rTime-lTime));
		dst.outSlope = qDelta;
		
		// Sample euler curves epsilon time before right key and get quaternion
		Quaternionf quat2 = EvaluateQuaternionFromEulerCurves(curveX, curveY, curveZ, lTime*0.001F + rTime * 0.999F);
		KeyframeTpl<Quaternionf>& dst2 = collapsed.GetKey(i+1);
		Quaternionf qDelta2 ((dst2.value.x - quat2.x) * 1000/(rTime-lTime),
							 (dst2.value.y - quat2.y) * 1000/(rTime-lTime),
							 (dst2.value.z - quat2.z) * 1000/(rTime-lTime),
							 (dst2.value.w - quat2.w) * 1000/(rTime-lTime));
		dst2.inSlope = qDelta2;
	}
	 
	collapsed.SetPreInfinity(curveX.GetPreInfinity());
	collapsed.SetPostInfinity(curveX.GetPostInfinity());

	collapsed.InvalidateCache();
	EnsureQuaternionContinuityPreserveSlope(collapsed);
}

template<class T>
void CombineCurve (const AnimationCurve& curve, int index, AnimationCurveTpl<T>& collapsed)
{
	int size, foundIndex;
	if (index == 0)
	{
		collapsed.SetPreInfinity(curve.GetPreInfinity());
		collapsed.SetPostInfinity(curve.GetPostInfinity());
	}

	// There is nothing in the collapsed curve yet
	// We will build it from scratch
	if (collapsed.GetKeyCount() == 0)
	{
		collapsed.ResizeUninitialized(curve.GetKeyCount());

		size = collapsed.GetKeyCount();
		for (int i=0;i<size;i++)
		{
			const AnimationCurve::Keyframe& src = curve.GetKey(i);
			KeyframeTpl<T>& dst = collapsed.GetKey(i);
			
			dst.time = src.time;

			dst.value = Zero<T>();
			dst.inSlope = Zero<T>();
			dst.outSlope = Zero<T>();
			// Write value into x,y,z,w axis based on index
			dst.value[index] = src.value;
			dst.inSlope[index] = src.inSlope;
			dst.outSlope[index] = src.outSlope;
		}
		
		collapsed.InvalidateCache();
		
		return;
	}
	
	// TODO : it has to be smaller than 0.002, because keyframes migth be closer to each other than that
	// we need some more advance technique to get this errorDelta or smarter way to match keyframes between curves
	float errorDelta = 0.000002F;
	//float errorDelta = 0.002F;
	
	AnimationCurve::Cache curveCache;
	typename AnimationCurveTpl<T>::Cache collapseCache;
	
	// Insert any new keys that are defined in the curve but are not in collapsed
	size = curve.GetKeyCount();
	for (int i=0;i<size;i++)
	{
		const AnimationCurve::Keyframe& src = curve.GetKey(i);
		if (collapsed.IsValid())
		{
			foundIndex = collapsed.FindIndex(collapseCache, src.time);
			const KeyframeTpl<T>& lhs = collapsed.GetKey (foundIndex);
			collapseCache.index = foundIndex;
			collapseCache.time = lhs.time;
			const KeyframeTpl<T>& rhs = collapsed.GetKey (foundIndex+1);
			
			// Do we have a key that is in the curve but not in the collapsed curve?
			// -> Add an inbetween
			if (!CompareApproximately(src.time, lhs.time, errorDelta) && !CompareApproximately(src.time, rhs.time, errorDelta))
			{
				collapsed.AddKey(CalculateInbetweenKey(lhs, rhs, src.time));	
				collapseCache.Invalidate();
			}
		}
		else
		{
			// We need at least two keyframes, for the AddInBetween key function to work
			AssertIf(collapsed.GetKeyCount () != 1);
			KeyframeTpl<T> copyKey = collapsed.GetKey (0);
			copyKey.time = src.time;
			collapsed.AddKey(copyKey);
			collapseCache.Invalidate();
		}
	}

	// Go through the dst keys.
	// Either copy from a key at the same time
	// or Calculate inbetween
	size = collapsed.GetKeyCount();
	for (int i=0;i<size;i++)
	{
		KeyframeTpl<T>& dst = collapsed.GetKey(i);
		KeyframeTpl<float> inbetween;
		
		if (curve.IsValid())
		{
			foundIndex = curve.FindIndex(curveCache, dst.time);
			const KeyframeTpl<float>& lhs = curve.GetKey (foundIndex);
			curveCache.index = foundIndex;
			curveCache.time = lhs.time;
			const KeyframeTpl<float>& rhs = curve.GetKey (foundIndex+1);
			
			if (CompareApproximately(dst.time, lhs.time, errorDelta))
				inbetween = lhs;
			else if (CompareApproximately(dst.time, rhs.time, errorDelta))
				inbetween = rhs;
			else
				inbetween = CalculateInbetweenKey(lhs, rhs, dst.time);
		}
		else
		{
			inbetween.value = curve.GetKeyCount() == 1 ? curve.GetKey (0).value : 0.0F;
			inbetween.inSlope = 0;
			inbetween.outSlope = 0;
		}

		dst.value[index] = inbetween.value;
		dst.inSlope[index] = inbetween.inSlope;
		dst.outSlope[index] = inbetween.outSlope;
	}
	
	collapsed.InvalidateCache();
}


#define CLIPPING_EPSILON (1.0F / 1000.0F)

template<class T>
void ValidateCurve (AnimationCurveTpl<T>& curve)
{
	if (!curve.IsValid())
		return;
	
	// validating that time of keyframes is increasing
	float t = -100000.0F;
	for (typename AnimationCurveTpl<T>::iterator i=curve.begin();i!=curve.end();i++)
	{
		AssertMsg(t < i->time, "Key frame placement is not increasing" );  // Would love to be able to specify the model here
		t = i->time;
	}
}

template<class T>
int FindClipKey (const AnimationCurveTpl<T>& curve, float time)
{
	const KeyframeTpl<T>* i = std::lower_bound (curve.begin (), curve.end (), time, KeyframeCompare());
	
	if (i == curve.end())
	{
		return curve.GetKeyCount() - 1;
	}
	else
	{
		int indexH = distance (curve.begin (), i);
		int indexL = max(indexH-1,0);	

		float diffH = fabs(curve.GetKey(indexH).time - time);
		float diffL = fabs(curve.GetKey(indexL).time - time);

		if(diffH < diffL)
		{
			return indexH;
		}
		else
		{
			return indexL;
		}
	}
}

template<class T>
bool ClipAnimationCurve (const AnimationCurveTpl<T>& sourceCurve, AnimationCurveTpl<T>& curve, float begin, float end)
{
	AssertIf(begin > end);
	dynamic_array<typename AnimationCurveTpl<T>::Keyframe> scratch;

	if (!sourceCurve.IsValid ())
	{
		return false;
	}
	
	pair<float, float> range = sourceCurve.GetRange();

	float offset = -begin;
	
	begin = clamp(begin, range.first, range.second);
	end   = clamp(end, range.first, range.second);

	// Contains no frames
	if (CompareApproximately(begin, end, CLIPPING_EPSILON))
	{
		return false;
	}
	
	int firstIndex = FindClipKey(sourceCurve, begin);
	int lastIndex = FindClipKey(sourceCurve, end);

	// 2 for the possible interpolated ones and one for extra nicenecess because usually we might 
	// add an extra looping frame later on
	scratch.reserve(std::max(lastIndex - firstIndex, 0) + 3);

	if (CompareApproximately (begin, sourceCurve.GetKey(firstIndex).time, CLIPPING_EPSILON))
	{
		scratch.push_back(sourceCurve.GetKey(firstIndex));
		firstIndex++;
	}
	else
	{
		scratch.push_back(CalculateInbetweenKey(sourceCurve, begin));
		if(begin > sourceCurve.GetKey(firstIndex).time) firstIndex++;
	}

	if (CompareApproximately (end, sourceCurve.GetKey(lastIndex).time, CLIPPING_EPSILON))
	{
		scratch.push_back(sourceCurve.GetKey(lastIndex));
	}
	else
	{
		scratch.push_back(CalculateInbetweenKey(sourceCurve, end));
		if(end > sourceCurve.GetKey(lastIndex).time) lastIndex++;
	}
	
	// Insert all inbetween keys
	if (lastIndex > firstIndex)
		scratch.insert(scratch.begin()+1, sourceCurve.begin() + firstIndex, sourceCurve.begin() + lastIndex);
	
	// Zero base the clipped animation
	for (unsigned int i=0;i<scratch.size();i++)
		scratch[i].time += offset;

	curve.Assign(scratch.begin(), scratch.end());
	curve.InvalidateCache();
	
	ValidateCurve(curve);

	AssertMsg(curve.GetKeyCount() >= 2, "Key count: %d", curve.GetKeyCount());

	return true;
}

void EnsureQuaternionContinuityPreserveSlope (AnimationCurveQuat& curve)
{
	if (!curve.IsValid())
		return;
	
	int keyCount = curve.GetKeyCount ();
	
	Quaternionf last (curve.GetKey (keyCount-1).value);
	for (int i=0;i<keyCount;i++)
	{
		Quaternionf cur (curve.GetKey (i).value);
		if (Dot (cur, last) < 0.0F)
		{
			cur = Quaternionf (-cur.x, -cur.y, -cur.z, -cur.w);
			curve.GetKey (i).value = cur;
			curve.GetKey (i).inSlope = -curve.GetKey (i).inSlope;
			curve.GetKey (i).outSlope = -curve.GetKey (i).outSlope;
		}
		last = cur;
	}
}

void EnsureQuaternionContinuityAndRecalculateSlope (AnimationCurveQuat& curve)
{
	if (!curve.IsValid())
		return;
		
	int keyCount = curve.GetKeyCount ();
	
	Quaternionf last (curve.GetKey (keyCount-1).value);
	for (int i=0;i<keyCount;i++)
	{
		Quaternionf cur (curve.GetKey (i).value);
		if (Dot (cur, last) < 0.0F)
			cur = Quaternionf (-cur.x, -cur.y, -cur.z, -cur.w);
		last = cur;
		curve.GetKey (i).value = cur;
	}
	
	for (int i=0;i<keyCount;i++)
		RecalculateSplineSlopeT (curve, i);
}

template<class T>
void RecalculateSplineSlope (AnimationCurveTpl<T>& curve)
{
	for (int i=0;i<curve.GetKeyCount ();i++)
		RecalculateSplineSlopeT (curve, i);
}

template<class T>
void RecalculateSplineSlopeLinear (AnimationCurveTpl<T>& curve)
{
	if (curve.GetKeyCount () < 2)
		return;

	for (int i=0;i<curve.GetKeyCount () - 1;i++)
	{
		RecalculateSplineSlopeLinear( curve, i );
	}
}

template<class T>
void RecalculateSplineSlopeLinear (AnimationCurveTpl<T>& curve, int key)
{
	AssertIf(key < 0 || key >= curve.GetKeyCount() - 1);
	if (curve.GetKeyCount () < 2)
		return;

	float dx = curve.GetKey (key).time - curve.GetKey (key+1).time;
	T dy = curve.GetKey (key).value - curve.GetKey (key+1).value;
	T m = dy / dx;
	curve.GetKey (key).outSlope = m;
	curve.GetKey (key+1).inSlope = m;
}

void RecalculateSplineSlope (AnimationCurveTpl<float>& curve, int key, float bias)
{
	RecalculateSplineSlopeT<float>(curve, key, bias);
}

template<class T>
void RecalculateSplineSlopeT (AnimationCurveTpl<T>& curve, int key, float b)
{
	AssertIf(key < 0 || key >= curve.GetKeyCount());
	if (curve.GetKeyCount () < 2)
		return;
	
	// First keyframe
	// in and out slope are set to be the slope from this to the right key
	if (key == 0)
	{
		float dx = curve.GetKey (1).time - curve.GetKey (0).time;
		T dy = curve.GetKey (1).value - curve.GetKey (0).value;
		T m = dy / dx;
		curve.GetKey (key).inSlope = m; curve.GetKey (key).outSlope = m;
	}
	// last keyframe
	// in and out slope are set to be the slope from this to the left key
	else if (key == curve.GetKeyCount () - 1)
	{
		float dx = curve.GetKey (key).time - curve.GetKey (key-1).time;
		T dy = curve.GetKey (key).value - curve.GetKey (key-1).value;
		T m = dy / dx;
		curve.GetKey (key).inSlope = m; curve.GetKey (key).outSlope = m;
	}
	// Keys are on the left and right
	// Calculates the slopes from this key to the left key and the right key.
	// Then blend between them using the bias
	// A bias of zero doesn't bend in any direction
	// a positive bias bends to the right
	else
	{
		float dx1 = curve.GetKey (key).time - curve.GetKey (key-1).time;
		T dy1 = curve.GetKey (key).value - curve.GetKey (key-1).value;

		float dx2 = curve.GetKey (key+1).time - curve.GetKey (key).time;
		T dy2 = curve.GetKey (key+1).value - curve.GetKey (key).value;
		
		T m1 = SafeDeltaDivide(dy1, dx1);
		T m2 = SafeDeltaDivide(dy2, dx2);
		
		T m = (1.0F + b) * 0.5F * m1 + (1.0F - b) * 0.5F * m2;
		curve.GetKey (key).inSlope = m; curve.GetKey (key).outSlope = m;
	}
	
	curve.InvalidateCache ();
}


template<class T>
void RecalculateSplineSlopeLoop (AnimationCurveTpl<T>& curve, int key, float b)
{
	AssertIf(key < 0 || key >= curve.GetKeyCount());
	if (curve.GetKeyCount () < 2)
		return;
	
	int keyPrev = key - 1;
	int keyNext = key + 1;
	if (key == 0)
		keyPrev = curve.GetKeyCount() - 2;
	else if (key+1 == curve.GetKeyCount())
		keyNext = 1;
	else
		AssertString("Not supported");
	
	// Keys are on the left and right
	// Calculates the slopes from this key to the left key and the right key.
	// Then blend between them using the bias
	// A bias of zero doesn't bend in any direction
	// a positive bias bends to the right
	float dx1 = curve.GetKey (key).time - curve.GetKey (keyPrev).time;
	T dy1 = curve.GetKey (key).value - curve.GetKey (keyPrev).value;

	float dx2 = curve.GetKey (keyNext).time - curve.GetKey (key).time;
	T dy2 = curve.GetKey (keyNext).value - curve.GetKey (key).value;
	
	T m1 = SafeDeltaDivide(dy1, dx1);
	T m2 = SafeDeltaDivide(dy2, dx2);
	
	T m = (1.0F + b) * 0.5F * m1 + (1.0F - b) * 0.5F * m2;
	curve.GetKey (key).inSlope = m; curve.GetKey (key).outSlope = m;
	
	curve.InvalidateCache ();
}


template<class T>
void AddLoopingFrame (AnimationCurveTpl<T>& curve, float time)
{
	if (!curve.IsValid())
		return;
		
	KeyframeTpl<T> key;
	key.time = time;
	key.value = curve.GetKey(0).value;
	key.inSlope = curve.GetKey(0).outSlope;
	key.outSlope = curve.GetKey(0).outSlope;
	
	curve.AddKey(key);
	
	RecalculateSplineSlopeLoop(curve, 0, 0);
	RecalculateSplineSlopeLoop(curve, curve.GetKeyCount()-1, 0);
}

void EnsureQuaternionContinuityLoopFrame (AnimationCurveQuat& curve)
{
	if( curve.GetKeyCount () < 2 )
		return;
		
	int keyCount = curve.GetKeyCount ();
	
	Quaternionf last (curve.GetKey (keyCount-2).value);
	Quaternionf cur (curve.GetKey (keyCount-1).value);
	if (Dot (cur, last) < 0.0F)
		cur = Quaternionf (-cur.x, -cur.y, -cur.z, -cur.w);
	curve.GetKey (keyCount-1).value = cur;
	
	RecalculateSplineSlopeLoop(curve, keyCount-1, 0);
}


int AddKeySmoothTangents (AnimationCurve& curve, float time, float value)
{
	AnimationCurve::Keyframe key;
	key.time = time;
	key.value = value;
	int index = curve.AddKey (key);
	if (index == -1)
		return -1;
	
	// Recalculate spline slope of this and the two keyframes around us!
	if (index > 0)
		RecalculateSplineSlope (curve, index - 1, 0.0F);
	RecalculateSplineSlope (curve, index, 0.0F);
	if (index + 1 < curve.GetKeyCount ())
		RecalculateSplineSlope (curve, index + 1, 0.0F);
	
	return index;
}



template<class T>
T InterpolateKeyframe (const KeyframeTpl<T>& lhs, const KeyframeTpl<T>& rhs, float curveT)
{
	float dx = rhs.time - lhs.time;
	T m1;
	T m2;
	float t;

	if (dx != 0.0F)
	{
		t = (curveT - lhs.time) / dx;
		m1 = lhs.outSlope * dx;
		m2 = rhs.inSlope * dx;
	}
	else
	{
		t = 0.0F;
		m1 = Zero<T>();
		m2 = Zero<T>();
	}

	return HermiteInterpolate (t, lhs.value, m1, m2, rhs.value);
}

int UpdateCurveKey (AnimationCurve& curve, int index, const AnimationCurve::Keyframe& value)
{
	float time = curve.GetKey(index).time;
	if ((index-1 < 0 || index+1 < curve.GetKeyCount()) &&
		time > curve.GetKey(index-1).time && 
  	    time < curve.GetKey(index+1).time)
	{
		curve.GetKey(index) = value; 
		return index;
	}
	else
	{
		curve.RemoveKeys(curve.begin() + index, curve.begin() + index + 1);
		return curve.AddKey(value);
	}
}

int MoveCurveKey (AnimationCurve& curve, int index, AnimationCurve::Keyframe value)
{
	float time = curve.GetKey(index).time;
	
	curve.RemoveKeys(curve.begin() + index, curve.begin() + index + 1);
	int newCloseIndex = curve.FindIndex(value.time);
	
	if (newCloseIndex >= 0)
	{
		Assert(curve.GetKeyCount() > 0);
	
		// Too close to some keyframes -> Keep time of the old time value
		if((newCloseIndex - 1 >= 0 &&					Abs(value.time - curve.GetKey(clamp(newCloseIndex-1, 0, curve.GetKeyCount()-1)).time) < kCurveTimeEpsilon) ||
														Abs(value.time - curve.GetKey(clamp(newCloseIndex  , 0, curve.GetKeyCount()-1)).time) < kCurveTimeEpsilon ||
		   (newCloseIndex + 1 < curve.GetKeyCount() &&	Abs(value.time - curve.GetKey(clamp(newCloseIndex+1, 0, curve.GetKeyCount()-1)).time) < kCurveTimeEpsilon) ||
														Abs(value.time - curve.GetKey(curve.GetKeyCount()-1).time) < kCurveTimeEpsilon)
		{
			value.time = time;
		}
	}
		
	return curve.AddKey(value);
}

// Calculates Hermite curve coefficients
void HermiteCooficients (double t, double& a, double& b, double& c, double& d)
{
	double t2 = t * t;
	double t3 = t2 * t;

	a = 2.0F * t3 - 3.0F * t2 + 1.0F;
	b = t3 - 2.0F * t2 + t;
	c = t3 - t2;
	d = -2.0F * t3 +  3.0F * t2;
}

namespace TToArray
{
	template <class T> float& Index(T& value, int index) { return value[index]; }
	template <class T> float  Index(const T& value, int index) { return value[index]; }

	template <> float& Index<float>(float& value, int index) 
	{ 
		AssertIf(index != 0);
		return value; 
	}
	template <> float  Index<float>(const float& value, int index) 
	{ 
		AssertIf(index != 0);
		return value;
	}

	template <class T> int CoordinateCount();
	template <> int CoordinateCount<float>() { return 1; }	
	template <> int CoordinateCount<Vector3f>() { return 3; }
	template <> int CoordinateCount<Quaternionf>() { return 4; }
}

template <class T>
void FitTangents(KeyframeTpl<T>& key0, KeyframeTpl<T>& key1, float time1, float time2, const T& value1, const T& value2)
{
	AssertIf(fabsf(time1) < std::numeric_limits<float>::epsilon());
	AssertIf(fabsf(time2) < std::numeric_limits<float>::epsilon());

	const float dt = key1.time - key0.time;

	const int coordinateCount = TToArray::CoordinateCount<T>();

	if (fabsf(dt) < std::numeric_limits<float>::epsilon())
	{
		for (int i = 0; i < coordinateCount; ++i)
		{
			TToArray::Index(key0.outSlope, i) = 0;
			TToArray::Index(key1.inSlope, i)  = 0;
		}
	}
	else
	{
		// p0 and p1 for Hermite curve interpolation equation
		const T p0 = key0.value;
		const T p1 = key1.value;

		// Hermite coefficients at points time1 and time2
		double a1, b1, c1, d1;
		double a2, b2, c2, d2;

		// TODO : try using doubles, because it doesn't work well when p0==p1==v0==v1
		HermiteCooficients(time1, a1, b1, c1, d1);
		HermiteCooficients(time2, a2, b2, c2, d2);

		for (int i = 0; i < coordinateCount; ++i)
		{
			// we need to solve these two equations in order to find m0 and m1
			// b1 * m0 + c1 * m1 = v0 - a1 * p0 - d1 * p1;
			// b2 * m0 + c2 * m1 = v1 - a2 * p0 - d2 * p1;

			// c1, c2 is never equal 0, because time1 and time2 not equal to 0

			// divide by c1 and c2
			// b1 / c1 * m0 + m1 = (v0 - a1 * p0 - d1 * p1) / c1;
			// b2 / c2 * m0 + m1 = (v1 - a2 * p0 - d2 * p1) / c2;

			// subtract one from another
			// b1 / c1 * m0 - b2 / c2 * m0 = (v0 - a1 * p0 - d1 * p1) / c1 - (v1 - a2 * p0 - d2 * p1) / c2;

			// solve for m0
			// (b1 / c1 - b2 / c2) * m0 = (v0 - a1 * p0 - d1 * p1) / c1 - (v1 - a2 * p0 - d2 * p1) / c2;

			const double v0 = TToArray::Index(value1, i);
			const double v1 = TToArray::Index(value2, i);
			const double pp0 = TToArray::Index(p0, i);
			const double pp1 = TToArray::Index(p1, i);
			
			// calculate m0
			const double m0 = ((v0 - a1 * pp0 - d1 * pp1) / c1 - (v1 - a2 * pp0 - d2 * pp1) / c2) / (b1 / c1 - b2 / c2);

			// solve for m1 using m0
			// c1 * m1 = p0 - a1 * p0 - d1 * p1 - b1 * m0;

			// calculate m1
			const double m1 = (v0 - a1 * pp0 - d1 * pp1 - b1 * m0) / c1;

			TToArray::Index(key0.outSlope, i) = static_cast<float>(m0 / dt);
			TToArray::Index(key1.inSlope, i)  = static_cast<float>(m1 / dt);
		}
	}
}


// Instantiate templates
template void RecalculateSplineSlope (AnimationCurveTpl<float>& curve);

template bool ClipAnimationCurve (const AnimationCurveTpl<float>& sourceCurve, AnimationCurveTpl<float>& curve, float begin, float end);
template bool ClipAnimationCurve (const AnimationCurveTpl<Quaternionf>& sourceCurve, AnimationCurveTpl<Quaternionf>& curve, float begin, float end);
template bool ClipAnimationCurve (const AnimationCurveTpl<Vector3f>& sourceCurve, AnimationCurveTpl<Vector3f>& curve, float begin, float end);

template void CombineCurve (const AnimationCurve& curve, int index, AnimationCurveTpl<Vector3f>& collapsed);
template void CombineCurve (const AnimationCurve& curve, int index, AnimationCurveTpl<Quaternionf>& collapsed);

template void AddLoopingFrame (AnimationCurveTpl<float>& curve, float time);
template void AddLoopingFrame (AnimationCurveTpl<Quaternionf>& curve, float time);
template void AddLoopingFrame (AnimationCurveTpl<Vector3f>& curve, float time);

template void RecalculateSplineSlopeLoop (AnimationCurveTpl<float>& curve, int key, float b);
template void RecalculateSplineSlopeLoop (AnimationCurveTpl<Quaternionf>& curve, int key, float b);
template void RecalculateSplineSlopeLoop (AnimationCurveTpl<Vector3f>& curve, int key, float b);

template void RecalculateSplineSlopeLinear (AnimationCurveTpl<float>& curve);
template void RecalculateSplineSlopeLinear (AnimationCurveTpl<float>& curve, int key);

template float InterpolateKeyframe (const KeyframeTpl<float>& lhs, const KeyframeTpl<float>& rhs, float curveT);
template Vector3f InterpolateKeyframe (const KeyframeTpl<Vector3f>& lhs, const KeyframeTpl<Vector3f>& rhs, float curveT);
template Quaternionf InterpolateKeyframe (const KeyframeTpl<Quaternionf>& lhs, const KeyframeTpl<Quaternionf>& rhs, float curveT);

template void FitTangents(KeyframeTpl<float>& key0, KeyframeTpl<float>& key1, float time1, float time2, const float& value1, const float& value2);
template void FitTangents(KeyframeTpl<Vector3f>& key0, KeyframeTpl<Vector3f>& key1, float time1, float time2, const Vector3f& value1, const Vector3f& value2);
template void FitTangents(KeyframeTpl<Quaternionf>& key0, KeyframeTpl<Quaternionf>& key1, float time1, float time2, const Quaternionf& value1, const Quaternionf& value2);

