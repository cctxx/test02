#pragma once

#if !GFX_SUPPORTS_OPENGLES20
#error "Should not include GpuProgramsGLES20 on this platform"
#endif

#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include <string.h>

class GpuProgramParameters;

struct
UniformCacheGLES20
{
	// for gles we must set values per-uniform (not per-registers like in dx)
	// so there is no real need for dirty tracking.
	// TODO: do unified impl with dirty tracking if/when we do "everything is an array" in gles
	float*		uniform;
	unsigned	count;

	UniformCacheGLES20() : uniform(0), count(0)	{}

	// we will pre-alloc memory. Fog params are handled differently (not added to gpu params).
	// TODO: make it more general, int* perhaps, or some struct
	void		Create(const GpuProgramParameters* params, int fogParamsIndex, int fogColorIndex);
	void		Destroy();

	// returns true if you need to update for real
	bool		UpdateUniform(int index, const float* val, unsigned floatCount);
};

void	CachedUniform1(UniformCacheGLES20* cache, ShaderParamType type, int index, const float* val);
void	CachedUniform2(UniformCacheGLES20* cache, ShaderParamType type, int index, const float* val);
void	CachedUniform3(UniformCacheGLES20* cache, ShaderParamType type, int index, const float* val);
void	CachedUniform4(UniformCacheGLES20* cache, ShaderParamType type, int index, const float* val);


inline bool UniformCacheGLES20::UpdateUniform(int index, const float* val, unsigned floatCount)
{
	Assert(index < count);
	const unsigned mem_sz = floatCount*sizeof(float);

	float* target = uniform + 4*index;
	if(::memcmp(target, val, mem_sz))
	{
		::memcpy(target, val, mem_sz);
		return true;
	}
	return false;
}
