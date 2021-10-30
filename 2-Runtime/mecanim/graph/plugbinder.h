/*
 Copyright (c) 7244339 Canada Inc. (Mecanim)
 All Rights Reserved.
*/
#pragma once

#include "Runtime/mecanim/bind.h"
#include "Runtime/mecanim/graph/graph.h"
#include "Runtime/mecanim/generic/valuearray.h"

namespace mecanim
{
	typedef binder2<function<void (graph::GraphPlug const*, uint32_t, ValueArray const*, graph::EvaluationGraphWorkspace*)>::ptr,
					graph::GraphPlug const*,
					uint32_t> 
			SetPlugBinder2;

	typedef binder2<function<void (graph::GraphPlug const*, uint32_t, ValueArray*, graph::EvaluationGraphWorkspace*)>::ptr,
					graph::GraphPlug const*, 
					uint32_t > 
			GetPlugBinder2;

	typedef binder3<function<void (graph::GraphPlug const*, uint32_t, uint32_t, ValuesList const&, graph::EvaluationGraphWorkspace*)>::ptr,
					graph::GraphPlug const*,
					uint32_t,
					uint32_t> 
			SetPlugBinder3;


	typedef binder3<function<void (graph::GraphPlug const*, uint32_t, uint32_t, ValuesList const&, graph::EvaluationGraphWorkspace*)>::ptr,
					graph::GraphPlug const*,
					uint32_t,
					uint32_t> 
			GetPlugBinder3;	

	STATIC_INLINE void SetPlugValue(mecanim::graph::GraphPlug const* plug, mecanim::uint32_t valueIndex, mecanim::ValueArray const* valueArray, mecanim::graph::EvaluationGraphWorkspace* graphWS)
	{	
		char ATTRIBUTE_ALIGN(ALIGN4F) value[48];
		valueArray->m_ValueArray[valueIndex]->ReadData(value);
		plug->WriteData( value, graphWS->m_EvaluationInfo);
	}
	STATIC_INLINE void GetPlugValue(mecanim::graph::GraphPlug const* plug, mecanim::uint32_t valueIndex, mecanim::ValueArray* valueArray, mecanim::graph::EvaluationGraphWorkspace* graphWS)
	{	
		char ATTRIBUTE_ALIGN(ALIGN4F) value[48];
		plug->ReadData( value, graphWS->m_EvaluationInfo);
		valueArray->m_ValueArray[valueIndex]->WriteData(value);	
	}

	STATIC_INLINE void SetPlugValueList(mecanim::graph::GraphPlug const* plug, mecanim::uint32_t listIndex, mecanim::uint32_t valueIndex, ValuesList const& valuesList, mecanim::graph::EvaluationGraphWorkspace* graphWS)
	{	
		char ATTRIBUTE_ALIGN(ALIGN4F) value[48];
		valuesList.m_InValues[listIndex]->m_ValueArray[valueIndex]->ReadData(value);
		plug->WriteData( value, graphWS->m_EvaluationInfo);
	}
	STATIC_INLINE void GetPlugValueList(mecanim::graph::GraphPlug const* plug, mecanim::uint32_t listIndex, mecanim::uint32_t valueIndex, ValuesList const& valuesList, mecanim::graph::EvaluationGraphWorkspace* graphWS)
	{	
		char ATTRIBUTE_ALIGN(ALIGN4F) value[48];
		plug->ReadData( value, graphWS->m_EvaluationInfo);
		valuesList.m_OutValues[listIndex]->m_ValueArray[valueIndex]->WriteData(value);	
	}

	void InitializeSetPlugBinder3(uint32_t& arCount, 
									SetPlugBinder3 *& arSetPlugBinder3, 
									ValuesList const& arValuesList, 
									graph::EvaluationGraph const* apGraph, 
									memory::Allocator& arAlloc);

	void InitializeGetPlugBinder3(uint32_t& arCount, 
									GetPlugBinder3 *& arGetPlugBinder3, 
									ValuesList const& arValuesList, 
									graph::EvaluationGraph const* apGraph, 
									memory::Allocator& arAlloc);

	void InitializeSetPlugBinder2(uint32_t& arCount, 
									SetPlugBinder2 *& arSetPlugBinder2, 
									ValuesList const& arValuesList, 
									graph::EvaluationGraph const* apGraph,
									memory::Allocator& arAlloc);

	void InitializeGetPlugBinder2(uint32_t& arCount, 
									GetPlugBinder2 *& arGetPlugBinder2, 
									ValuesList const& arValuesList,
									graph::EvaluationGraph const* apGraph,
									memory::Allocator& arAlloc);
}