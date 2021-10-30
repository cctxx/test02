#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/Serialize/Blobification/offsetptr.h"


namespace mecanim
{
namespace animation
{
struct ConstantClip
{
	UInt32              curveCount;
	OffsetPtr<float>    data;
	
	ConstantClip () : curveCount (0) { }
	
	DEFINE_GET_TYPESTRING(ConstantClip)
	
	template<class TransferFunction>
	inline void Transfer (TransferFunction& transfer)
	{			
		TRANSFER_BLOB_ONLY(curveCount);
		MANUAL_ARRAY_TRANSFER2(float, data, curveCount);
	}
};
	
// Sample functions
void                SampleClip                 (const ConstantClip& curveData, uint32_t outputCount, float* output);
float               SampleClipAtIndex          (const ConstantClip& curveData, int index);

void                DestroyConstantClip       (ConstantClip& clip, memory::Allocator& alloc);
void                CreateConstantClip        (ConstantClip& clip, size_t curveCount, memory::Allocator& alloc);

}
}