#include "UnityPrefix.h"
#include "denseclip.h"
#include "Runtime/Math/Simd/math.h"
namespace mecanim
{
namespace animation
{
		
static void BlendArray(const float* lhs, const float* rhs, size_t size, float t, float* output)
{
	for (int i=0;i<size;i++)
		output[i] = math::lerp(lhs[i], rhs[i], t); 
}
	
	
static void  PrepareBlendValues (const DenseClip& clip, float time, float*& lhs, float*& rhs, float& u)	
{
	time -= clip.m_BeginTime;
	
	float index;
	u = math::modf(time * clip.m_SampleRate, index);

	int lhsIndex = (int)index;
	int rhsIndex = lhsIndex + 1;
	lhsIndex = math::maximum(0, lhsIndex);
	lhsIndex = math::minimum(clip.m_FrameCount-1, lhsIndex);
	
	rhsIndex = math::maximum(0, rhsIndex);
	rhsIndex = math::minimum(clip.m_FrameCount-1, rhsIndex);
	
	lhs = const_cast<float*> (&clip.m_SampleArray[lhsIndex * clip.m_CurveCount]);
	rhs = const_cast<float*> (&clip.m_SampleArray[rhsIndex * clip.m_CurveCount]);
}

void  SampleClip                 (const DenseClip& clip, float time, float* output)
{
	float u;
	float* lhsPtr;
	float* rhsPtr;
	PrepareBlendValues(clip, time, lhsPtr, rhsPtr, u);
	
	BlendArray(lhsPtr, rhsPtr, clip.m_CurveCount, u, output);
}

float SampleClipAtIndex          (const DenseClip& clip, int curveIndex, float time)
{
	float u;
	float* lhsPtr;
	float* rhsPtr;
	PrepareBlendValues(clip, time, lhsPtr, rhsPtr, u);
	
	return math::lerp(lhsPtr[curveIndex], rhsPtr[curveIndex], u); 
}


// Creation & Destruction
void                DestroyDenseClip        (DenseClip& clip, memory::Allocator& alloc)
{
	alloc.Deallocate(clip.m_SampleArray);
}

}
}
