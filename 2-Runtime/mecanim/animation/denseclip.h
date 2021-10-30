#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/Serialize/Blobification/offsetptr.h"


namespace mecanim
{
	namespace animation
	{
		struct DenseClip
		{
			int                     m_FrameCount;
			uint32_t                m_CurveCount;
			float                   m_SampleRate;
			float                   m_BeginTime;
			
			uint32_t                m_SampleArraySize;
			OffsetPtr<float>		m_SampleArray;
			
			
			DenseClip () : m_FrameCount (0),m_CurveCount(0),m_SampleRate(0.0F),m_SampleArraySize(0),m_BeginTime(0.0F) { }
			
			DEFINE_GET_TYPESTRING(DenseClip)
			
			template<class TransferFunction>
			inline void Transfer (TransferFunction& transfer)
			{			
				TRANSFER(m_FrameCount);
				TRANSFER(m_CurveCount);
				TRANSFER(m_SampleRate);
				TRANSFER(m_BeginTime);

				TRANSFER_BLOB_ONLY(m_SampleArraySize);
				MANUAL_ARRAY_TRANSFER2(float, m_SampleArray, m_SampleArraySize);
			}
		};
		
		// Sample functions
		void                SampleClip                 (const DenseClip& curveData, float time, float* output);
		float               SampleClipAtIndex          (const DenseClip& curveData, int index, float time);
		
		
		// Creation & Destruction
		void                DestroyDenseClip        (DenseClip& clip, memory::Allocator& alloc);
	}
}