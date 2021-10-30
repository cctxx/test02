#include "UnityPrefix.h"
#include "StreamedClipBuilder.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/mecanim/memory.h"

struct BuildCurveKey
{
	float time;
	int   curveIndex;
	float coeff[4];
	
	friend bool operator < (const BuildCurveKey& lhs, const BuildCurveKey& rhs)
	{
		// Sort by time primarily
		if (lhs.time != rhs.time)
			return lhs.time < rhs.time;
		// for same time, Sort by curve index. This reduces cache trashing when sampling the clip
		else
			return lhs.curveIndex < rhs.curveIndex;
	}
};

struct StreamedClipBuilder
{
	StreamedClipBuilder(): allKeys(kMemTempAlloc) {}

	dynamic_array<BuildCurveKey> allKeys;
	int                          curveCount;
};

StreamedClipBuilder* CreateStreamedClipBuilder(UInt32 curveCount, UInt32 keyCount)
{
	StreamedClipBuilder* builder = UNITY_NEW(StreamedClipBuilder, kMemTempAlloc);
	builder->allKeys.reserve(keyCount);
	builder->curveCount = curveCount;
	return builder;
}

void DestroyStreamedClipBuilder(StreamedClipBuilder* builder)
{
	UNITY_DELETE(builder, kMemTempAlloc);
}

template<typename T>
void ConvertCacheToBuildKeys(typename AnimationCurveTpl<T>::Cache& cache, int curveIndex, StreamedClipBuilder* builder)
{
	int elements = sizeof(T)/sizeof(float);
	//Loop over number of floats in curveType
	for(int e = 0; e < elements; e++)
	{
		BuildCurveKey& key = builder->allKeys.push_back();

		key.time = cache.time;
		key.curveIndex = curveIndex+e;
		key.coeff[0] = cache.coeff[0][e];
		key.coeff[1] = cache.coeff[1][e];
		key.coeff[2] = cache.coeff[2][e];
		key.coeff[3] = cache.coeff[3][e];
	}
}

template<>
void ConvertCacheToBuildKeys<float>(AnimationCurveTpl<float>::Cache& cache, int curveIndex, StreamedClipBuilder* builder)
{
	BuildCurveKey& key = builder->allKeys.push_back();

	key.time = cache.time;
	key.curveIndex = curveIndex;
	key.coeff[0] = cache.coeff[0];
	key.coeff[1] = cache.coeff[1];
	key.coeff[2] = cache.coeff[2];
	key.coeff[3] = cache.coeff[3];
}

const float kFirstClampKeyframe = -FLT_MAX;

template<class T>
void AddCurveToStreamedClip(StreamedClipBuilder* builder, int curveIndex, const AnimationCurveTpl<T>& curve)
{
	for(int k = -1; k < curve.GetKeyCount(); k++)
	{
		typename AnimationCurveTpl<T>::Cache cache;

		// Last key needs to be specifically prepared as a constant value
		// Use clamp cache function to get same functionaliy as AnimationCurve.EvaluateClamp
		if (k == curve.GetKeyCount()-1)
		{
			cache.time = curve.GetKey(k).time;
			cache.coeff[0] = cache.coeff[1] = cache.coeff[2] = Zero<T>();
			cache.coeff[3] = curve.GetKey(k).value;
		}
		// A special first key needs to be created so that all values can be sampled before their first keyframe.
		else if (k == -1)
		{
			// We already have a key at the first firstClampKeyframe no need to duplicate it.
			if (kFirstClampKeyframe == curve.GetKey(0).time)
				continue;

			cache.time = kFirstClampKeyframe;
			cache.coeff[0] = cache.coeff[1] = cache.coeff[2] = Zero<T>();
			cache.coeff[3] = curve.GetKey(0).value;
		}
		// Use CalculateCacheData for all normal curve segements
		else
			curve.CalculateCacheData(cache, k, k+1, 0.0F);

		ConvertCacheToBuildKeys<T>(cache, curveIndex, builder);
	}
}

void AddIntegerCurveToStreamedClip(StreamedClipBuilder* builder, int curveIndex, float* time, int* value, int count)
{
	for (int k = 0; k < count; ++k)
	{
		AnimationCurveTpl<float>::Cache cache;

		// Last key needs to be specifically prepared as a constant value
		// Use clamp cache function to get same functionaliy as AnimationCurve.EvaluateClamp
		cache.time = (k == 0) ? kFirstClampKeyframe : time[k];
		cache.coeff[0] = cache.coeff[1] = cache.coeff[2] = 0.0f;
		cache.coeff[3] = value[k];

		ConvertCacheToBuildKeys<float>(cache, curveIndex, builder);
	}
}



template void AddCurveToStreamedClip<float>(StreamedClipBuilder* builder, int curveIndex, const AnimationCurveTpl<float>& curve);
template void AddCurveToStreamedClip<Vector3f>(StreamedClipBuilder* builder, int curveIndex, const AnimationCurveTpl<Vector3f>& curve);
template void AddCurveToStreamedClip<Quaternionf>(StreamedClipBuilder* builder, int curveIndex, const AnimationCurveTpl<Quaternionf>& curve);

template<class T>
T& PushData (dynamic_array<UInt8>& output)
{
	output.resize_uninitialized(output.size() + sizeof(T));
	return *reinterpret_cast<T*> (&output[output.size() - sizeof(T)]);
}

void CreateStreamClipConstant (StreamedClipBuilder* builder, mecanim::animation::StreamedClip& clip, mecanim::memory::Allocator& alloc)
{
	Assert(clip.curveCount == 0);
	Assert(clip.data.IsNull());
	
	std::sort(builder->allKeys.begin(), builder->allKeys.end());
	
	dynamic_array<UInt8> streamData;
	streamData.reserve((builder->allKeys.size()+1) * (sizeof(mecanim::animation::CurveKey) + sizeof(mecanim::animation::CurveTimeData)));
	
	
	// Generate the curvedata stream
	float currentTime = -std::numeric_limits<float>::infinity();
	for (int i=0;i<builder->allKeys.size();)
	{
		currentTime = builder->allKeys[i].time;
		mecanim::animation::CurveTimeData& timeData = PushData<mecanim::animation::CurveTimeData> (streamData);
		timeData.time = currentTime;
		
		int count = 0;
		while (i < builder->allKeys.size() && builder->allKeys[i].time == currentTime)
		{
			mecanim::animation::CurveKey& curveKey = PushData<mecanim::animation::CurveKey>(streamData);
			curveKey.curveIndex = builder->allKeys[i].curveIndex;
			memcpy(curveKey.coeff, builder->allKeys[i].coeff, sizeof(curveKey.coeff));
			
			i++;
			count++;
		}
		
		timeData.count = count;
	}
	
	// Make sure that we do not sample beyond the last actual key by adding an infinity key.
	mecanim::animation::CurveTimeData& timeData = PushData<mecanim::animation::CurveTimeData> (streamData);
	timeData.time = std::numeric_limits<float>::infinity();
	timeData.count = 0;
	
	clip.dataSize = streamData.size() / sizeof(mecanim::uint32_t);
	clip.data = alloc.ConstructArray<mecanim::uint32_t> (clip.dataSize);
	memcpy(clip.data.Get(), streamData.begin(), streamData.size());
	clip.curveCount = builder->curveCount;
}

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

typedef AnimationCurve::Keyframe Keyframe;

static float Evaluate0 (const mecanim::animation::StreamedClip& clip, mecanim::animation::StreamedClipMemory& memory, float time)
{
	float output;
	SampleClip(clip, memory, time, &output);
	
	return output;
}

typedef AnimationCurveVec3::Keyframe KeyframeVec3;
static Vector3f Evaluate3 (const mecanim::animation::StreamedClip& clip, mecanim::animation::StreamedClipMemory& memory, float time)
{
	Vector3f output;
	SampleClip(clip, memory, time, reinterpret_cast<float*>(&output));

	return output;
}

SUITE (StreamedClipBuilderTests)
{
TEST (StreamedClipBuilder_StreamedClipEvaluation)
{
	mecanim::memory::MecanimAllocator alloc(kMemTempAlloc);

	AnimationCurve curve;
	curve.AddKeyBackFast(Keyframe(0.5F, 0.0F));
	curve.AddKeyBackFast(Keyframe(1.0F, 1.0F));
	curve.AddKeyBackFast(Keyframe(2.0F, -1.0F));

	StreamedClipBuilder* builder = CreateStreamedClipBuilder(1, curve.GetKeyCount());
	AddCurveToStreamedClip(builder, 0, curve);
	mecanim::animation::StreamedClip streamclip;
	CreateStreamClipConstant (builder, streamclip, alloc);

	mecanim::animation::StreamedClipMemory memory;
	CreateStreamedClipMemory(streamclip, memory, alloc);

	CHECK_EQUAL(curve.EvaluateClamp(-5.0), Evaluate0(streamclip, memory, -5.0F));
	CHECK_EQUAL(curve.EvaluateClamp(1.0F), Evaluate0(streamclip, memory, 1.0F));
	CHECK_EQUAL(curve.EvaluateClamp(0.0F), Evaluate0(streamclip, memory, 0.0F));
	CHECK_EQUAL(curve.EvaluateClamp(1.5F), Evaluate0(streamclip, memory, 1.5F));
	CHECK_EQUAL(curve.EvaluateClamp(2.0F), Evaluate0(streamclip, memory, 2.0F));
	CHECK_EQUAL(curve.EvaluateClamp(0.1F), Evaluate0(streamclip, memory, 0.1F));
	CHECK_EQUAL(curve.EvaluateClamp(100.0F), Evaluate0(streamclip, memory, 100.0F));
	CHECK_EQUAL(curve.EvaluateClamp(-19), Evaluate0(streamclip, memory, -19.0F));
	
	DestroyStreamedClipMemory (memory, alloc);
	DestroyStreamedClip (streamclip, alloc);
	DestroyStreamedClipBuilder (builder);
}


TEST (StreamedClipEvaluationVector3)
{
	mecanim::memory::MecanimAllocator alloc(kMemTempAlloc);

	AnimationCurveVec3 curve;
	curve.AddKeyBackFast(KeyframeVec3(0.5F, Vector3f(0.0,1.0,2.0)));
	curve.AddKeyBackFast(KeyframeVec3(1.0F, Vector3f(3.0,0.0,4.0)));
	curve.AddKeyBackFast(KeyframeVec3(2.0F, Vector3f(0.0,-1.0,-2.0)));

	StreamedClipBuilder* builder = CreateStreamedClipBuilder(3, curve.GetKeyCount()*3);
	AddCurveToStreamedClip(builder, 0, curve);
	mecanim::animation::StreamedClip streamclip;
	CreateStreamClipConstant (builder, streamclip, alloc);

	mecanim::animation::StreamedClipMemory memory;
	CreateStreamedClipMemory(streamclip, memory, alloc);

	CHECK(curve.EvaluateClamp(-5.0) == Evaluate3(streamclip, memory, -5.0F));
	CHECK(curve.EvaluateClamp(1.0F) == Evaluate3(streamclip, memory, 1.0F));
	CHECK(curve.EvaluateClamp(0.0F) == Evaluate3(streamclip, memory, 0.0F));
	CHECK(curve.EvaluateClamp(1.5F) == Evaluate3(streamclip, memory, 1.5F));
	CHECK(curve.EvaluateClamp(2.0F) == Evaluate3(streamclip, memory, 2.0F));
	CHECK(curve.EvaluateClamp(0.1F) == Evaluate3(streamclip, memory, 0.1F));
	CHECK(curve.EvaluateClamp(100.0F) == Evaluate3(streamclip, memory, 100.0F));
	CHECK(curve.EvaluateClamp(-19) == Evaluate3(streamclip, memory, -19.0F));

	DestroyStreamedClipMemory (memory, alloc);
	DestroyStreamedClip (streamclip, alloc);
	DestroyStreamedClipBuilder (builder);
}
}
#endif
