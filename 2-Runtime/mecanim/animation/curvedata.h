#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h" 
#include "Runtime/mecanim/generic/valuearray.h"

#include "Runtime/Serialize/Blobification/offsetptr.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Animation/MecanimArraySerialization.h"
#include "streamedclip.h"
#include "denseclip.h"
#include "constantclip.h"

namespace mecanim
{
	
namespace animation
{
	struct Clip
	{
		DEFINE_GET_TYPESTRING(Clip)

		Clip() {}

		StreamedClip					m_StreamedClip;
		DenseClip                       m_DenseClip;
		ConstantClip                    m_ConstantClip;
		
		OffsetPtr<ValueArrayConstant>	m_DeprecatedBinding;
		
		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{			
			TRANSFER (m_StreamedClip);
			TRANSFER (m_DenseClip);
			TRANSFER (m_ConstantClip);
			
			transfer.Transfer(m_DeprecatedBinding, "m_Binding");
		}
	};

	struct ClipInput
	{
		float	m_Time;
	};

	struct ClipMemory
	{	
		StreamedClipMemory m_StreamedClipCache;
		int                m_ConstantClipValueCount;
	};

	struct ClipOutput
	{
	public:
		float* m_Values;
		
	};

	Clip* CreateClipSimple(uint32_t count, memory::Allocator& alloc);	
	void DestroyClip(Clip* clip, memory::Allocator& alloc);

	ClipMemory* CreateClipMemory(Clip const* clip, memory::Allocator& alloc);
	ClipMemory* CreateClipMemory(Clip const* clip, int32_t totalUsedCurves, memory::Allocator& alloc);	
	void DestroyClipMemory(ClipMemory* memory, memory::Allocator& alloc);	

	ClipOutput* CreateClipOutput(int32_t usedConstantCurves, memory::Allocator& alloc);	
	ClipOutput* CreateClipOutput(Clip const* clip, memory::Allocator& alloc);

	void DestroyClipOutput(ClipOutput* output, memory::Allocator& alloc);	

	float EvaluateClipAtIndex(Clip const* clip, ClipInput const* input, ClipMemory* memory, uint32_t index);
	void EvaluateClip(Clip const* clip, ClipInput const* input, ClipMemory* memory, ClipOutput* output);
	
	inline size_t GetClipCurveCount (const Clip& clip) { return clip.m_StreamedClip.curveCount + clip.m_DenseClip.m_CurveCount + clip.m_ConstantClip.curveCount; }
	inline bool IsValidClip (const Clip& clip) { return GetClipCurveCount (clip) != 0; }
	

} // namespace animation

}
