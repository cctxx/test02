/*
 Copyright (c) 7244339 Canada Inc. (Mecanim)
 All Rights Reserved.
*/
#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/object.h"
#include "Runtime/mecanim/bitset.h"

#include "Runtime/mecanim/graph/plug.h"
#include "Runtime/mecanim/graph/node.h"
#include "Runtime/mecanim/graph/factory.h"

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Animation/MecanimArraySerialization.h" 

namespace mecanim
{

namespace graph
{
	// If somebody change a node interface, like adding a new plug, he should create a new class instead for his new node
	// because some user's file may still use the old node and won't have the data to handle new plug, unexpected behavior
	// can result if this rule is not respected

	struct Vertex : public Object
	{
		DEFINE_GET_TYPESTRING(Vertex)

		DeclareObject(Vertex)

		Vertex():m_Id(numeric_limits<uint32_t>::max_value){}
		uint32_t m_Id;
		uint32_t m_Binding;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_Id);
			TRANSFER(m_Binding);
		}
	};

	struct Edge : public Object
	{
		DEFINE_GET_TYPESTRING(Edge)

		DeclareObject(Edge)

		Edge():m_SourceId(numeric_limits<uint32_t>::max_value),m_DestinationId(numeric_limits<uint32_t>::max_value){}
		uint32_t m_SourceId;
		uint32_t m_DestinationId;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_SourceId);
			TRANSFER(m_DestinationId);
		}
	};

	struct ConstantEdge : public Object
	{
		DEFINE_GET_TYPESTRING(ConstantEdge)

		DeclareObject(ConstantEdge)

		ConstantEdge():m_Id(numeric_limits<uint32_t>::max_value){}
		union {
			float		m_FloatValue;
			uint32_t	m_UIntValue;
			int32_t		m_IntValue;
			bool		m_BoolValue;
		};
		math::float4	m_VectorValue;

		uint32_t m_Id;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_Id);
			TRANSFER(m_VectorValue);
			TRANSFER(m_UIntValue);
		}
	};

	struct ExternalEdge : public Object
	{
		DEFINE_GET_TYPESTRING(ExternalEdge)

		DeclareObject(ExternalEdge)

		ExternalEdge():m_Id(numeric_limits<uint32_t>::max_value), m_Binding(numeric_limits<uint32_t>::max_value){}
		uint32_t m_Id;
		uint32_t m_Binding;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_Id);
			TRANSFER(m_Binding);
		}
	};

	struct Graph : public Object
	{
		DEFINE_GET_TYPESTRING(Graph)

		DeclareObject(Graph)

		Graph()
			:m_VerticesCount(0),m_Vertices(0),
			m_EdgesCount(0),m_Edges(0),
			m_InEdgesCount(0),m_InEdges(0),
			m_OutEdgesCount(0),m_OutEdges(0),
			m_ConstantCount(0),m_ConstantEdges(0)
		{}

		uint32_t		m_VerticesCount;
		Vertex*			m_Vertices;

		uint32_t		m_EdgesCount;
		Edge*			m_Edges;

		uint32_t		m_InEdgesCount;
		ExternalEdge*	m_InEdges;

		uint32_t		m_OutEdgesCount;
		ExternalEdge*	m_OutEdges;

		uint32_t		m_ConstantCount;
		ConstantEdge*	m_ConstantEdges;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			MANUAL_ARRAY_TRANSFER(mecanim::graph::Vertex, m_Vertices, m_VerticesCount);
			MANUAL_ARRAY_TRANSFER(mecanim::graph::Edge, m_Edges, m_EdgesCount);
			MANUAL_ARRAY_TRANSFER(mecanim::graph::ExternalEdge, m_InEdges, m_InEdgesCount);
			MANUAL_ARRAY_TRANSFER(mecanim::graph::ExternalEdge, m_OutEdges, m_OutEdgesCount);
			MANUAL_ARRAY_TRANSFER(mecanim::graph::ConstantEdge, m_ConstantEdges, m_ConstantCount);
		}
	};

	struct EvaluationGraph
	{
	public:
		EvaluationGraph():m_NodeCount(0),m_NodeArray(0),m_Input(0),m_Output(0),m_ConstantCount(0),m_ConstantArray(0){}

		Graph			m_Graph;

		uint32_t		m_NodeCount;
		Node**			m_NodeArray;

		GraphInput*		m_Input;
		GraphOutput*	m_Output;

		uint32_t		m_ConstantCount;
		Constant**		m_ConstantArray; 
	};

	struct EvaluationGraphWorkspace
	{
	public:
		EvaluationGraphWorkspace(){}

		EvaluationInfo m_EvaluationInfo;
	};

	EvaluationGraph*	CreateEvaluationGraph(Node** apNodeArray, uint32_t aNodeCount, 
							GraphPlug** apInputPlugArray, uint32_t aInputPlugCount, 
							GraphPlug** apOutputPlugArray, uint32_t aOutputPlugCount, 
							Constant** apConstantArray, uint32_t aConstantCount,
							memory::Allocator& arAlloc);

	EvaluationGraph*	CreateEvaluationGraph(Graph* apGraph, GraphFactory const& arFactory, memory::Allocator& arAlloc);

	void				DestroyEvaluationGraph(EvaluationGraph* apGraph, memory::Allocator& arAlloc);

	EvaluationGraphWorkspace*	CreateEvaluationGraphWorkspace(EvaluationGraph* apGraph, memory::Allocator& arAlloc);
	void					DestroyEvaluationGraphWorkspace(EvaluationGraphWorkspace* apGraphWorkspace, memory::Allocator& arAlloc);
}

}
