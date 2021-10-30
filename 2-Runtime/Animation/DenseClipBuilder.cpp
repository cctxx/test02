#include "UnityPrefix.h"
#include "DenseClipBuilder.h"
#include "Runtime/mecanim/memory.h"

void CreateDenseClip(mecanim::animation::DenseClip& clip, UInt32 curveCount, float begin, float end, float sampleRate, mecanim::memory::Allocator& alloc)
{
	// We always need at least one frame to sample the clip
	clip.m_FrameCount = std::max(CeilfToInt ((end - begin) * sampleRate), 1);
	clip.m_CurveCount = curveCount;
	clip.m_SampleRate = sampleRate;
	clip.m_BeginTime = begin;
	
	clip.m_SampleArraySize = clip.m_FrameCount * clip.m_CurveCount;
	clip.m_SampleArray = alloc.ConstructArray<float>(clip.m_SampleArraySize);
}

template<class T>
void AddCurveToDenseClip(mecanim::animation::DenseClip& clip, int curveIndex, const AnimationCurveTpl<T>& curve)
{
	for (int i=0;i<clip.m_FrameCount;i++)
	{
		float time = clip.m_BeginTime + ((float)i / clip.m_SampleRate);

		float* dst = &clip.m_SampleArray[i * clip.m_CurveCount + curveIndex];
		
		T value = curve.EvaluateClamp(time);
		*reinterpret_cast<T*> (dst) = value;
		
	}
}

template void AddCurveToDenseClip<float>(mecanim::animation::DenseClip& clip, int curveIndex, const AnimationCurveTpl<float>& curve);
template void AddCurveToDenseClip<Vector3f>(mecanim::animation::DenseClip& clip, int curveIndex, const AnimationCurveTpl<Vector3f>& curve);
template void AddCurveToDenseClip<Quaternionf>(mecanim::animation::DenseClip& clip, int curveIndex, const AnimationCurveTpl<Quaternionf>& curve);

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

typedef AnimationCurveVec3::Keyframe KeyframeVec3;
static Vector3f Evaluate3 (const mecanim::animation::DenseClip& clip, float time)
{
	float output[4];
	SampleClip(clip, time, output);

	return Vector3f(output[1], output[2], output[3]);
}

typedef AnimationCurve::Keyframe Keyframe;
static float Evaluate1 (const mecanim::animation::DenseClip& clip, float time)
{
	float output;
	output = SampleClipAtIndex(clip, time, 0);
	return output;
}


SUITE (DenseClipBuilderTests)
{
TEST (DenseClipBuilder_EvaluationVector3)
{
	mecanim::memory::MecanimAllocator alloc(kMemTempAlloc);

	AnimationCurveVec3 curve;
	curve.AddKeyBackFast(KeyframeVec3(0.5F, Vector3f(0.0,1.0,2.0)));
	curve.AddKeyBackFast(KeyframeVec3(1.0F, Vector3f(3.0,0.0,4.0)));
	curve.AddKeyBackFast(KeyframeVec3(2.0F, Vector3f(0.0,-1.0,-2.0)));
	
	AnimationCurve curve2;
	curve2.AddKeyBackFast(Keyframe(0.3F, 1.0));
	curve2.AddKeyBackFast(Keyframe(1.2F, 20.0));
	curve2.AddKeyBackFast(Keyframe(2.3F, 5.0));
	
	
	mecanim::animation::DenseClip clip;
	CreateDenseClip (clip, 3+1, std::min(curve.GetRange().first, curve2.GetRange().first), std::max(curve.GetRange().second, curve2.GetRange().second), 30.0F, alloc);
	AddCurveToDenseClip(clip, 0, curve2);
	AddCurveToDenseClip(clip, 1, curve);
	
	float kEpsilon = 0.001F;

	CHECK(CompareApproximately(curve.EvaluateClamp(1.5F),   Evaluate3(clip, 1.5F), kEpsilon));

	
	CHECK(CompareApproximately(curve.EvaluateClamp(-5.0),   Evaluate3(clip, -5.0F), kEpsilon));
	CHECK(CompareApproximately(curve.EvaluateClamp(-5.0),   Evaluate3(clip, -5.0F), kEpsilon));
	CHECK(CompareApproximately(curve.EvaluateClamp(0.0F),   Evaluate3(clip, 0.0F), kEpsilon));
	CHECK(CompareApproximately(curve.EvaluateClamp(1.439F), Evaluate3(clip, 1.439F), kEpsilon));
	CHECK(CompareApproximately(curve.EvaluateClamp(2.0F),   Evaluate3(clip, 2.0F), kEpsilon));
	CHECK(CompareApproximately(curve.EvaluateClamp(0.1F),   Evaluate3(clip, 0.1F), kEpsilon));
	CHECK(CompareApproximately(curve.EvaluateClamp(100.0F), Evaluate3(clip, 100.0F), kEpsilon));
	CHECK(CompareApproximately(curve.EvaluateClamp(-19),    Evaluate3(clip, -19.0F), kEpsilon));

	DestroyDenseClip (clip, alloc);
}
}
#endif
