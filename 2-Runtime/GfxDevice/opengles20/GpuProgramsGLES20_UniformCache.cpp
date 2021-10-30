#include "UnityPrefix.h"
#if !GFX_SUPPORTS_OPENGLES20
#error "Should not include GpuProgramsGLES20 on this platform"
#endif

#include "GpuProgramsGLES20_UniformCache.h"

#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/GfxDevice/GpuProgram.h"
#include "IncludesGLES20.h"
#include "AssertGLES20.h"


void UniformCacheGLES20::Create(const GpuProgramParameters* params, int fogParamsIndex, int fogColorIndex)
{
	int lastUsedUniform = -1;

	// we will track only float/vector uniforms
	GpuProgramParameters::ValueParameterArray::const_iterator paramI	= params->GetValueParams().begin();
	GpuProgramParameters::ValueParameterArray::const_iterator paramEnd	= params->GetValueParams().end();
	while(paramI != paramEnd)
	{
		if(paramI->m_RowCount == 1 && paramI->m_ArraySize == 1 && paramI->m_Index > lastUsedUniform)
			lastUsedUniform = paramI->m_Index;

		++paramI;
	}

	const BuiltinShaderParamIndices& builtinParam = params->GetBuiltinParams();
	for(unsigned i = 0 ; i < kShaderInstanceVecCount ; ++i)
	{
		if(builtinParam.vec[i].gpuIndex > lastUsedUniform)
			lastUsedUniform = builtinParam.vec[i].gpuIndex;
	}

	if(fogParamsIndex > lastUsedUniform)	lastUsedUniform = fogParamsIndex;
	if(fogColorIndex > lastUsedUniform)		lastUsedUniform = fogColorIndex;

	count   = lastUsedUniform + 1;
	uniform = (float*)UNITY_MALLOC_ALIGNED(kMemShader, count*4 * sizeof(float), 16);
	memset(uniform, 0xff /* NaN */, count*4 * sizeof(float));
}

void UniformCacheGLES20::Destroy()
{
	count = 0;

	UNITY_FREE(kMemShader, uniform);
	uniform = 0;
}


// In theory Uniform*f can also be used to load bool uniforms, in practice
// some drivers don't like that (e.g. PVR GLES2.0 Emu wants bools to be loaded
// via Uniform*i). So load both integers and bools via *i functions.
#define CACHED_UNIFORM_IMPL(Count) \
void CachedUniform##Count(UniformCacheGLES20* cache, ShaderParamType type, int index, const float* val)	\
{																					\
	if (cache->UpdateUniform(index, val, Count)) {									\
		if (type == kShaderParamFloat)												\
			GLES_CHK(glUniform##Count##fv(index, 1, val));							\
		else {																		\
			int ival[4] = {val[0],val[1],val[2],val[3]};							\
			GLES_CHK(glUniform##Count##iv(index, 1, ival));							\
		}																			\
	}																				\
}																					\

CACHED_UNIFORM_IMPL(1);
CACHED_UNIFORM_IMPL(2);
CACHED_UNIFORM_IMPL(3);
CACHED_UNIFORM_IMPL(4);
