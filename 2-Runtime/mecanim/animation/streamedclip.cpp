#include "UnityPrefix.h"
#include "streamedclip.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/Utilities/Prefetch.h"
#include "Runtime/Math/Simd/math.h"

namespace mecanim
{
namespace animation
{	
	
	
inline static float EvaluateCache (const StreamedCacheItem& cache, float sampleTime)
{
	float t = sampleTime - cache.time;
	return (t * (t * (t * cache.coeff[0] + cache.coeff[1]) + cache.coeff[2])) + cache.coeff[3];
}

inline static void EvaluateMultipleCaches ( const StreamedCacheItem& cache0,
								   const StreamedCacheItem& cache1,
								   const StreamedCacheItem& cache2,
								   const StreamedCacheItem& cache3,
								   float sampleTime, float* output)
{
	const math::float4 time(sampleTime);
	
	const math::float4 cachetime(cache0.time, cache1.time, cache2.time, cache3.time);
	const math::float4 dt = time - cachetime;
	
	const math::float4 coeffs0(cache0.coeff[0], cache1.coeff[0], cache2.coeff[0], cache3.coeff[0]);
	const math::float4 coeffs1(cache0.coeff[1], cache1.coeff[1], cache2.coeff[1], cache3.coeff[1]);
	const math::float4 coeffs2(cache0.coeff[2], cache1.coeff[2], cache2.coeff[2], cache3.coeff[2]);
	const math::float4 coeffs3(cache0.coeff[3], cache1.coeff[3], cache2.coeff[3], cache3.coeff[3]);
		
	ATTRIBUTE_ALIGN(ALIGN4F) float v[4];
	math::store(dt * (dt * (dt * coeffs0 + coeffs1) + coeffs2) + coeffs3, v);

	output[0] = v[0];
	output[1] = v[1];
	output[2] = v[2];
	output[3] = v[3];
}
	
static void EvaluateCaches (const StreamedClipMemory& cache, float sampleTime, float* output)
{
	const StreamedCacheItem* caches = &cache.caches[0];
	Prefetch(caches);
	Prefetch(caches+2);
	Prefetch(caches+4);
	Prefetch(caches+6);
	Prefetch(caches+8);
	
	int i = 0;
	for ( ; i+4 <= cache.cacheCount; i+=4, caches+=4 )
	{
		Prefetch(caches+10);
		Prefetch(caches+12);
		const StreamedCacheItem& item0 = *(caches);
		const StreamedCacheItem& item1 = *(caches+1);
		const StreamedCacheItem& item2 = *(caches+2);
		const StreamedCacheItem& item3 = *(caches+3);
		EvaluateMultipleCaches(item0, item1, item2, item3, sampleTime, &output[i]);
	}
	
	for ( ; i<cache.cacheCount; ++i, ++caches)
	{
		const StreamedCacheItem& item = *caches;
		output[i] = EvaluateCache(item, sampleTime);
	}
	
}
	
static void RewindCache (StreamedClipMemory& cache)
{
	cache.time = -std::numeric_limits<float>::infinity();
	cache.readByteOffset = 0;
}

static inline void ConsumeCurveTimeData (const CurveTimeData* __restrict curveData, StreamedCacheItem* __restrict caches)
{
	float time = curveData->time;
	const CurveKey* __restrict keys = reinterpret_cast<const CurveKey*> (curveData + 1);
	int count = curveData->count;
	
	Prefetch(keys,0);
	Prefetch(keys+3);

	int curveIndex = keys[0].curveIndex;
	float coeff0 = keys[0].coeff[0];
	float coeff1 = keys[0].coeff[1];
	float coeff2 = keys[0].coeff[2];
	float coeff3 = keys[0].coeff[3];
	
	for (int i=1;i<count;i++)
	{
		Prefetch(keys+3+i);

		StreamedCacheItem& activeCache = caches[curveIndex];
		activeCache.time = time;
		activeCache.coeff[0] = coeff0;
		activeCache.coeff[1] = coeff1;
		activeCache.coeff[2] = coeff2;
		activeCache.coeff[3] = coeff3;
		curveIndex = keys[i].curveIndex;
		coeff0 = keys[i].coeff[0];
		coeff1 = keys[i].coeff[1];
		coeff2 = keys[i].coeff[2];
		coeff3 = keys[i].coeff[3];
	}
	StreamedCacheItem& activeCache = caches[curveIndex];
	activeCache.time = time;
	activeCache.coeff[0] = coeff0;
	activeCache.coeff[1] = coeff1;
	activeCache.coeff[2] = coeff2;
	activeCache.coeff[3] = coeff3;
}

static void SeekClipForward (const UInt8* curveData, float time, StreamedClipMemory& cache)
{
	int readByteOffset = cache.readByteOffset;
	const CurveTimeData* data = reinterpret_cast<const CurveTimeData*> (curveData + readByteOffset);

	while (time >= data->time)
	{
		// Consume the data and apply it to the cache
		ConsumeCurveTimeData(data, cache.caches);
		
		// Seek forward by the consumed data
		readByteOffset += sizeof(CurveTimeData) + data->count * sizeof(CurveKey);

		data = reinterpret_cast<const CurveTimeData*> (curveData + readByteOffset);
	}
	
	// Synchronize cached time & offset
	cache.time = time;
	cache.readByteOffset = readByteOffset;
}

void SeekClip (const StreamedClip& curveData, StreamedClipMemory& cache, float time)	
{
	Assert(cache.cacheCount == curveData.curveCount);

	// No seeking is necessary, we are exactly at the same cached time
	// (Happens due to SampleClipAtIndex)
	// @TODO: it would be best to remove that and instead seperate root motion data from other data
	if (time == cache.time)
		return;
	
	// Seeking backwards is not supported. Jump the beginning of curve.
	if (time < cache.time)
		RewindCache(cache);
	
	// Seek and make sure the curve cache is up to date
	const UInt8* stream = reinterpret_cast<const UInt8*> (curveData.data.Get());
	SeekClipForward(stream, time, cache);
}
	
void SampleClip (const StreamedClip& curveData, StreamedClipMemory& cache, float time, float* output)
{
	SeekClip(curveData, cache, time);
	
	// Evaluate the cache and write sampled values to output
	EvaluateCaches(cache, time, output);
}

float SampleClipAtIndex (const StreamedClip& curveData, StreamedClipMemory& cache, int index, float time)
{
	Assert(index < curveData.curveCount);

	SeekClip(curveData, cache, time);
	
	return EvaluateCache(cache.caches[index], time);
}
	
void CreateStreamedClipMemory(const StreamedClip& clip, StreamedClipMemory& mem, memory::Allocator& alloc)
{
	SETPROFILERLABEL(StreamedClipMemory);

	mem.caches = alloc.ConstructArray<StreamedCacheItem>(clip.curveCount);
	mem.cacheCount = clip.curveCount;
	
	RewindCache(mem);
}

void DestroyStreamedClipMemory (StreamedClipMemory& memory, memory::Allocator& alloc)
{
	alloc.Deallocate(memory.caches);
}
	
void DestroyStreamedClip (StreamedClip& clip, memory::Allocator& alloc)
{
	alloc.Deallocate(clip.data);
}

}
}
