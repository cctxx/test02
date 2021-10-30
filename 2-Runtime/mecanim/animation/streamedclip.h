#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/Serialize/Blobification/offsetptr.h"


namespace mecanim
{
namespace animation
{

// StreamedClip is a stream oriented clip format.
// It stores hermite coefficients directly. Multiple curves are stored in the same stream. Keys for all curves are sorted by time.
// This drastically reduces cache misses. In the best case we can sample a clip with a single cache miss. Clip data is read completely linearly.

// Instead of laying out the animationclip by curves containing any array of keys.
// We sort all keys of all curves by time and each key has an index.
// This is a streamed format basically CurveTimeData defines the time value and the number of keys at this time.
// Then  CurveTimeData.count CurveKey elements will follow.

// time = 0
// CurveTimeData(4)  ... CurveKey . CurveKey . CurveKey .  CurveKey

// time = 0.2
// CurveTimeData(1)  ... CurveKey
	
// Sampling is separated into two functions.
// 1. Seeking, it is responsible for updating the cached in the StreamedClipMemory to ensure each curve Index has an up to date time and hermite cofficients.
// 2. Evaluating the caches. This simply evaluates the hermite caches
	
// See StreamedClipBuilder.cpp and AnimationClip.cpp on how to build the streamed clip data.	
	
struct StreamedCacheItem
{
	float  time;
	float  coeff[4];
};

struct StreamedClipMemory
{
	StreamedCacheItem* caches;
	int                cacheCount;
	float              time;
	UInt32             readByteOffset;
};

struct StreamedClip
{
	uint32_t            dataSize;
	OffsetPtr<uint32_t> data;
	UInt32              curveCount;
	
	
	StreamedClip () : curveCount (0),dataSize(0) { }
	
	DEFINE_GET_TYPESTRING(StreamedClip)
	
	template<class TransferFunction>
	inline void Transfer (TransferFunction& transfer)
	{			
		TRANSFER_BLOB_ONLY(dataSize);
		MANUAL_ARRAY_TRANSFER2(uint32_t, data, dataSize);
		TRANSFER(curveCount);
	}
};
	
struct CurveTimeData
{	
	float  time;
	UInt32 count;
	// This is implicitly followed by CurveKey in the stream
};

struct CurveKey
{
	int    curveIndex;
	float  coeff[4];
};


// Sample functions
void                SampleClip                 (const StreamedClip& curveData, StreamedClipMemory& cache, float time, float* output);
float               SampleClipAtIndex          (const StreamedClip& curveData, StreamedClipMemory& cache, int index, float time);

	
// Creation & Destruction
// StreamedClipBuilder.h actually creates the streamed clip from an array of AnimationCurve.
void                CreateStreamedClipMemory   (const StreamedClip& clip, StreamedClipMemory& memory, memory::Allocator& alloc);
void                DestroyStreamedClipMemory  (StreamedClipMemory& memory, memory::Allocator& alloc);
void                DestroyStreamedClip        (StreamedClip& clip, memory::Allocator& alloc);

}
}