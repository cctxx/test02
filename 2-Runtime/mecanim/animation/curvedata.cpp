#include "UnityPrefix.h"
#include "Runtime/mecanim/animation/curvedata.h"

namespace mecanim
{

namespace animation
{
	Clip* CreateClipSimple(uint32_t count, memory::Allocator& alloc)
	{	
		SETPROFILERLABEL(Clip);

		// Allocate data.
		Clip* clip = alloc.Construct<Clip>();
		return clip;				
	}
	

	void DestroyClip(Clip* clip, memory::Allocator& alloc)
	{
		if(clip)
		{
			DestroyStreamedClip(clip->m_StreamedClip, alloc);
			DestroyDenseClip(clip->m_DenseClip, alloc);

			alloc.Deallocate(clip);
		}
	}	

	ClipMemory* CreateClipMemory(Clip const* clip, memory::Allocator& alloc)
	{
		return CreateClipMemory(clip, GetClipCurveCount(*clip), alloc);
	}

	ClipMemory* CreateClipMemory(Clip const* clip, int32_t totalUsedCurves, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ClipMemory);

		ClipMemory* mem = alloc.Construct<ClipMemory>();
		mem->m_ConstantClipValueCount = totalUsedCurves - (GetClipCurveCount(*clip) - clip->m_ConstantClip.curveCount);
		Assert(mem->m_ConstantClipValueCount >= 0 && mem->m_ConstantClipValueCount <= clip->m_ConstantClip.curveCount);
		
		CreateStreamedClipMemory(clip->m_StreamedClip, mem->m_StreamedClipCache, alloc);
		
		return mem;	
	}

	void DestroyClipMemory(ClipMemory* memory, memory::Allocator& alloc)
	{
		if(memory)
		{
			DestroyStreamedClipMemory(memory->m_StreamedClipCache, alloc);
			alloc.Deallocate(memory);
		}
	}

	ClipOutput* CreateClipOutput(Clip const* clip, memory::Allocator& alloc)
	{
		return CreateClipOutput(GetClipCurveCount(*clip), alloc);
	}
	
	ClipOutput* CreateClipOutput(int32_t totalUsedCurves, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ClipOutput);

		ClipOutput* out = alloc.Construct<ClipOutput>();
		out->m_Values = alloc.ConstructArray<float> (totalUsedCurves);
		
		return out;
	}
	
	void DestroyClipOutput(ClipOutput* output, memory::Allocator& alloc)
	{
		if(output)
		{
			alloc.Deallocate(output->m_Values);
			alloc.Deallocate(output);
		}
	}	

	float EvaluateClipAtIndex(Clip const* clip, ClipInput const* input, ClipMemory* memory, uint32_t index)
	{
		if (index < clip->m_StreamedClip.curveCount)
			return SampleClipAtIndex(clip->m_StreamedClip, memory->m_StreamedClipCache, index, input->m_Time);
		index -= clip->m_StreamedClip.curveCount;

		if (index < clip->m_DenseClip.m_CurveCount)
			return SampleClipAtIndex(clip->m_DenseClip, index, input->m_Time);
		index -= clip->m_DenseClip.m_CurveCount;
		
		return SampleClipAtIndex(clip->m_ConstantClip, index);
	}

	void EvaluateClip(Clip const* clip, ClipInput const* input, ClipMemory* memory, ClipOutput* output )
	{
		float* outputData = output->m_Values;

		if (clip->m_StreamedClip.curveCount != 0)
		{
			SampleClip(clip->m_StreamedClip, memory->m_StreamedClipCache, input->m_Time, outputData);
			outputData += clip->m_StreamedClip.curveCount;
		}

		if (clip->m_DenseClip.m_CurveCount != 0)
		{
			SampleClip(clip->m_DenseClip, input->m_Time, outputData);
			outputData += clip->m_DenseClip.m_CurveCount;
		}

		if (memory->m_ConstantClipValueCount != 0)
		{
			// Constant clips are not sampling based on the total curve count
			SampleClip(clip->m_ConstantClip, memory->m_ConstantClipValueCount, outputData);
		}
	}
}

}
