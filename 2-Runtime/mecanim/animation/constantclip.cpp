#include "UnityPrefix.h"
#include "constantclip.h"
#include "Runtime/mecanim/memory.h"

namespace mecanim
{
	namespace animation
	{	
		void SampleClip (const ConstantClip& curveData, uint32_t outputCount, float* output)
		{
			DebugAssert(outputCount <= curveData.curveCount);
			
			memcpy(output, curveData.data.Get(), outputCount * sizeof(float));
		}
		
		float SampleClipAtIndex (const ConstantClip& curveData, int index)
		{
			Assert(index < curveData.curveCount);
			
			return curveData.data[index];
		}
		
		void DestroyConstantClip (ConstantClip& clip, memory::Allocator& alloc)
		{
			alloc.Deallocate(clip.data);
		}

        void CreateConstantClip (ConstantClip& clip, size_t curveCount, memory::Allocator& alloc)
        {
            clip.curveCount = curveCount;
            clip.data = alloc.ConstructArray<float> (curveCount);
        }
	}
}
