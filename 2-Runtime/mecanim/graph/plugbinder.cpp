#include "UnityPrefix.h"
#include "Runtime/mecanim/graph/plugbinder.h"

namespace mecanim
{
	void InitializeSetPlugBinder3(uint32_t& arCount, SetPlugBinder3 *& arSetPlugBinder3, ValuesList const& arValuesList, graph::EvaluationGraph const* apGraph, memory::Allocator& arAlloc)
	{
		arCount = 0;
		arSetPlugBinder3 = 0;

		uint32_t i;
		for(i = 0 ; i < apGraph->m_Input->mPlugCount; i++)
		{
			graph::GraphPlug& plug = *apGraph->m_Input->mPlugArray[i];
			uint32_t j;
			for(j=0;j<arValuesList.m_InValuesCount;++j)
			{
				int32_t index = FindValueIndex(arValuesList.m_InValuesConstant[j], plug.m_ID);				
				if(index > -1)
				{
					arCount++;
				}
			}			
		}

		if(arCount)
		{
			arSetPlugBinder3 = arAlloc.ConstructArray<SetPlugBinder3>(arCount);
			uint32_t binderCount;
			for(i = 0, binderCount = 0; i < apGraph->m_Input->mPlugCount; i++)
			{
				graph::GraphPlug const* plug = apGraph->m_Input->mPlugArray[i];
				uint32_t j;
				for(j=0;j<arValuesList.m_InValuesCount;++j)
				{
					int32_t index = FindValueIndex(arValuesList.m_InValuesConstant[j], plug->m_ID);				
					if(index > -1)
					{
						uint32_t valueIndex = static_cast<uint32_t>(index);
						arSetPlugBinder3[binderCount++] = bind( SetPlugValueList, plug, j, valueIndex);
					}
				}				
			}
		}
	}

	void InitializeGetPlugBinder3(uint32_t& arCount, 
									GetPlugBinder3 *& arGetPlugBinder3, 
									ValuesList const& arValuesList, 
									graph::EvaluationGraph const* apGraph, 
									memory::Allocator& arAlloc)
	{
		arCount = 0;
		arGetPlugBinder3 = 0;

		uint32_t i;
		for(i = 0 ; i < apGraph->m_Output->mPlugCount; i++)
		{
			graph::GraphPlug& plug = *apGraph->m_Output->mPlugArray[i];
			uint32_t j;
			for(j=0;j<arValuesList.m_OutValuesCount;++j)
			{
				int32_t index = FindValueIndex(arValuesList.m_OutValuesConstant[j], plug.m_ID);				
				if(index > -1)
				{
					arCount++;
				}
			}
		}
		
		if(arCount)
		{
			arGetPlugBinder3 = arAlloc.ConstructArray<GetPlugBinder3>(arCount);
			uint32_t binderCount;
			for(i = 0, binderCount = 0; i < apGraph->m_Output->mPlugCount; i++)
			{
				graph::GraphPlug const* plug = apGraph->m_Output->mPlugArray[i];
				uint32_t j;
				for(j=0;j<arValuesList.m_OutValuesCount;++j)
				{
					int32_t index = FindValueIndex(arValuesList.m_OutValuesConstant[j], plug->m_ID);				
					if(index > -1)
					{	
						uint32_t valueIndex = static_cast<uint32_t>(index);
						arGetPlugBinder3[binderCount++] = bind( GetPlugValueList, plug, j, valueIndex);
					}
				}				
			}
		}
	}



	void InitializeSetPlugBinder2(uint32_t& arCount, SetPlugBinder2 *& arSetPlugBinder2, ValuesList const& arValuesList, graph::EvaluationGraph const* apGraph, memory::Allocator& arAlloc)
	{
		arCount = 0;
		arSetPlugBinder2 = 0;

		uint32_t i;
		for(i = 0 ; i < apGraph->m_Input->mPlugCount; i++)
		{
			graph::GraphPlug& plug = *apGraph->m_Input->mPlugArray[i];
			uint32_t j;
			for(j=0;j<arValuesList.m_InValuesCount;++j)
			{
				int32_t index = FindValueIndex(arValuesList.m_InValuesConstant[j], plug.m_ID);				
				if(index > -1)
				{
					arCount++;
				}
			}			
		}

		if(arCount)
		{
			arSetPlugBinder2 = arAlloc.ConstructArray<SetPlugBinder2>(arCount);
			uint32_t binderCount;
			for(i = 0, binderCount = 0; i < apGraph->m_Input->mPlugCount; i++)
			{
				graph::GraphPlug const* plug = apGraph->m_Input->mPlugArray[i];
				uint32_t j;
				for(j=0;j<arValuesList.m_InValuesCount;++j)
				{
					int32_t index = FindValueIndex(arValuesList.m_InValuesConstant[j], plug->m_ID);				
					if(index > -1)
					{
						uint32_t valueIndex = static_cast<uint32_t>(index);					
						arSetPlugBinder2[binderCount++] = bind( SetPlugValue, plug, valueIndex);
					}
				}				
			}
		}
	}

	void InitializeGetPlugBinder5(uint32_t& arCount, 
									GetPlugBinder2 *& arGetPlugBinder2, 
									ValuesList const& arValuesList, 
									graph::EvaluationGraph const* apGraph,
									memory::Allocator& arAlloc)
	{
		arCount = 0;
		arGetPlugBinder2 = 0;

		uint32_t i;
		for(i = 0 ; i < apGraph->m_Output->mPlugCount; i++)
		{
			graph::GraphPlug& plug = *apGraph->m_Output->mPlugArray[i];
			uint32_t j;
			for(j=0;j<arValuesList.m_OutValuesCount;++j)
			{
				int32_t index = FindValueIndex(arValuesList.m_OutValuesConstant[j], plug.m_ID);				
				if(index > -1)
				{
					arCount++;
				}
			}
		}
		
		if(arCount)
		{
			arGetPlugBinder2 = arAlloc.ConstructArray<GetPlugBinder2>(arCount);
			uint32_t binderCount;
			for(i = 0, binderCount = 0; i < apGraph->m_Output->mPlugCount; i++)
			{
				graph::GraphPlug const* plug = apGraph->m_Output->mPlugArray[i];
				uint32_t j;
				for(j=0;j<arValuesList.m_OutValuesCount;++j)
				{
					int32_t index = FindValueIndex(arValuesList.m_OutValuesConstant[j], plug->m_ID);				
					if(index > -1)
					{	
						uint32_t valueIndex = static_cast<uint32_t>(index);
						arGetPlugBinder2[binderCount++] = bind( GetPlugValue, plug, valueIndex);
					}
				}				
			}
		}
	}

}
