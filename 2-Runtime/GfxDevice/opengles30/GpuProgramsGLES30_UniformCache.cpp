#include "UnityPrefix.h"
#if !GFX_SUPPORTS_OPENGLES30
#error "Should not include GpuProgramsGLES30 on this platform"
#endif

#include "GpuProgramsGLES30_UniformCache.h"

#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/GfxDevice/GpuProgram.h"
#include "IncludesGLES30.h"
#include "AssertGLES30.h"


void UniformCacheGLES30::Create(const GpuProgramParameters* params, int fogParamsIndex, int fogColorIndex)
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

	count	= lastUsedUniform + 1;
	uniform	= (float*)UNITY_MALLOC_ALIGNED(kMemShader, count*4 * sizeof(float), 16);
	memset(uniform, 0xff /* NaN */, count*4 * sizeof(float));
}

void UniformCacheGLES30::Destroy()
{
	count = 0;

	UNITY_FREE(kMemShader, uniform);
	uniform = 0;
}

#define CACHED_UNIFORM_IMPL(Count)													\
void CachedUniform##Count(UniformCacheGLES30* cache, int index, const float* val)	\
{																					\
	if(cache->UpdateUniform(index, val, Count))										\
		GLES_CHK(glUniform##Count##fv(index, 1, val));								\
}																					\

CACHED_UNIFORM_IMPL(1);
CACHED_UNIFORM_IMPL(2);
CACHED_UNIFORM_IMPL(3);
CACHED_UNIFORM_IMPL(4);
