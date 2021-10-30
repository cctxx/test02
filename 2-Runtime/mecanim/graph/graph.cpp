#include "UnityPrefix.h"
#include "Runtime/Math/Simd/math.h"
#include "Runtime/mecanim/graph/graph.h"
#include "Runtime/mecanim/graph/factory.h"

namespace mecanim
{

namespace graph
{
	static void AssignPlugId(EvaluationGraph* apGraph)
	{
		// Assigning id to each graph plug
		uint32_t i, id = 0;
		for(i=0;i<apGraph->m_NodeCount;i++)
		{
			Node* node = apGraph->m_NodeArray[i];

			apGraph->m_Graph.m_Vertices[i].m_Id = node->NodeType();
			apGraph->m_Graph.m_Vertices[i].m_Binding = node->mID;
			uint32_t j;
			for(j=0;j<node->GetPlugCount();j++)
			{
				node->GetPlug(j).m_PlugId = id++;
			}			
		}

		for(i=0;i<apGraph->m_Input->GetPlugCount();i++)
		{
			apGraph->m_Input->GetPlug(i).m_PlugId = id++;
		}	

		for(i=0;i<apGraph->m_Output->GetPlugCount();i++)
		{
			apGraph->m_Output->GetPlug(i).m_PlugId = id++;		
		}
	}

	static void AssignPlugOffset(EvaluationGraph* apGraph, memory::Allocator& arAlloc)
	{
		// assigning value offset for each plug
		uint32_t i, offset = 0;		
		for(i=0;i<apGraph->m_NodeCount;i++)
		{
			Node* node = apGraph->m_NodeArray[i];			
			uint32_t j;
			for(j=0;j<node->GetPlugCount();j++)
			{
				GraphPlug& plug = node->GetPlug(j);
				offset = plug.m_Offset = arAlloc.AlignAddress(offset, plug.ValueAlign());
				offset += plug.ValueSize();
			}			
		}

		for(i=0;i<apGraph->m_Input->GetPlugCount();i++)
		{
			GraphPlug& plug = apGraph->m_Input->GetPlug(i);
			offset = plug.m_Offset = arAlloc.AlignAddress(offset, plug.ValueAlign());
			offset += plug.ValueSize();
		}

		for(i=0;i<apGraph->m_Output->GetPlugCount();i++)
		{
			GraphPlug& plug = apGraph->m_Output->GetPlug(i);
			offset = plug.m_Offset = arAlloc.AlignAddress(offset, plug.ValueAlign());
			offset += plug.ValueSize();
		}	
	}

	static void ConnectEdges(EvaluationGraph* apGraph)
	{
		uint32_t i, edgesIt = 0;
		for(i=0;i<apGraph->m_NodeCount;i++)
		{
			Node* node = apGraph->m_NodeArray[i];
			uint32_t j;
			for(j=0;j<node->GetPlugCount();j++)
			{
				GraphPlug* plug = node->GetPlug(j).GetSource();
				if(plug)
				{
					apGraph->m_Graph.m_Edges[edgesIt].m_SourceId = plug->m_PlugId;
					apGraph->m_Graph.m_Edges[edgesIt].m_DestinationId = node->GetPlug(j).m_PlugId;
					edgesIt++;
				}
			}
		}

		for(i=0;i<apGraph->m_Output->GetPlugCount();i++)
		{
			GraphPlug* plug = apGraph->m_Output->mPlugArray[i]->GetSource();
			if(plug)
			{
				apGraph->m_Graph.m_Edges[edgesIt].m_SourceId = plug->m_PlugId;
				apGraph->m_Graph.m_Edges[edgesIt].m_DestinationId = apGraph->m_Output->mPlugArray[i]->m_PlugId;
				edgesIt++;
			}	
		}
	}

	static EvaluationGraph* CreateGraph(Node** apNodeArray, uint32_t aNodeCount, 
								GraphPlug** apInputPlugArray, uint32_t aInputPlugCount, 
								GraphPlug** apOutputPlugArray, uint32_t aOutputPlugCount, 
								uint32_t aConnectionCount, 
								Constant** apConstantArray, uint32_t aConstantCount, 
								memory::Allocator& arAlloc)
	{	
		EvaluationGraph* cst = arAlloc.Construct<EvaluationGraph>();

		cst->m_NodeCount = aNodeCount;
		cst->m_NodeArray = arAlloc.ConstructArray<Node*>(aNodeCount);

		cst->m_Graph.m_VerticesCount = aNodeCount;
		cst->m_Graph.m_Vertices = arAlloc.ConstructArray<Vertex>(aNodeCount);

		cst->m_Graph.m_EdgesCount = aConnectionCount;
		cst->m_Graph.m_Edges = arAlloc.ConstructArray<Edge>(aConnectionCount);

		cst->m_Graph.m_InEdgesCount = aInputPlugCount;
		cst->m_Graph.m_InEdges = arAlloc.ConstructArray<ExternalEdge>(aInputPlugCount);

		cst->m_Graph.m_OutEdgesCount = aOutputPlugCount;
		cst->m_Graph.m_OutEdges = arAlloc.ConstructArray<ExternalEdge>(aOutputPlugCount);

		cst->m_Graph.m_ConstantCount = aConstantCount;		
		cst->m_Graph.m_ConstantEdges = arAlloc.ConstructArray<ConstantEdge>(cst->m_Graph.m_ConstantCount);
		
		cst->m_Input = arAlloc.Construct<GraphInput>();

		cst->m_Input->mPlugArray = arAlloc.ConstructArray<GraphPlug*>(aInputPlugCount);

		cst->m_Output = arAlloc.Construct<GraphOutput>();

		cst->m_Output->mPlugArray = arAlloc.ConstructArray<GraphPlug*>(aOutputPlugCount);

		cst->m_ConstantCount = aConstantCount;
		cst->m_ConstantArray = arAlloc.ConstructArray<Constant*>(aConstantCount);

		memcpy(&cst->m_NodeArray[0], &apNodeArray[0], sizeof(Node*)*aNodeCount);

		memcpy(&cst->m_ConstantArray[0], &apConstantArray[0], sizeof(Constant*)*aConstantCount);

		cst->m_Input->mPlugCount = aInputPlugCount;
		memcpy(&cst->m_Input->mPlugArray[0], &apInputPlugArray[0], sizeof(GraphPlug*)*aInputPlugCount);
		uint32_t i;
		for(i=0;i<aInputPlugCount;i++)
		{
			cst->m_Input->mPlugArray[i]->m_Owner = cst->m_Input;
			cst->m_Graph.m_InEdges[i].m_Id = GetPlugType(*apInputPlugArray[i]);
			cst->m_Graph.m_InEdges[i].m_Binding = apInputPlugArray[i]->m_ID;
		}

		cst->m_Output->mPlugCount = aOutputPlugCount;
		memcpy(&cst->m_Output->mPlugArray[0], &apOutputPlugArray[0], sizeof(GraphPlug*)*aOutputPlugCount);
		for(i=0;i<aOutputPlugCount;i++)
		{
			cst->m_Output->mPlugArray[i]->m_Owner = cst->m_Output;
			cst->m_Graph.m_OutEdges[i].m_Id = GetPlugType(*apOutputPlugArray[i]);
			cst->m_Graph.m_OutEdges[i].m_Binding = apOutputPlugArray[i]->m_ID;
		}

		AssignPlugId(cst);
		AssignPlugOffset(cst, arAlloc);

		for(i=0;i<aConstantCount;i++)
		{
			// copy the largest data element
			cst->m_Graph.m_ConstantEdges[i].m_FloatValue = cst->m_ConstantArray[i]->m_FloatValue;
			cst->m_Graph.m_ConstantEdges[i].m_VectorValue = cst->m_ConstantArray[i]->m_VectorValue;
			if(cst->m_ConstantArray[i]->m_Plug)
			{
				cst->m_Graph.m_ConstantEdges[i].m_Id = cst->m_ConstantArray[i]->m_Plug->m_PlugId;
			}
		}

		return cst;
	}

	EvaluationGraph* CreateEvaluationGraph(Node** apNodeArray, uint32_t aNodeCount, 
								GraphPlug** apInputPlugArray, uint32_t aInputPlugCount, 
								GraphPlug** apOutputPlugArray, uint32_t aOutputPlugCount, 
								Constant** apConstantArray, uint32_t aConstantCount,
								memory::Allocator& arAlloc)
	{
		uint32_t connectionCount = 0;
		uint32_t i;
		for(i=0;i<aNodeCount;i++)
		{
			Node* node = apNodeArray[i];
			uint32_t j;
			for(j=0;j<node->GetPlugCount();j++)
			{
				GraphPlug* plug = node->GetPlug(j).GetSource();
				if(plug)
				{
					connectionCount++;
				}
			}
		}

		for(i=0;i<aOutputPlugCount;i++)
		{
			GraphPlug* plug = apOutputPlugArray[i]->GetSource();
			if(plug)
			{
				connectionCount++;
			}
		}

		EvaluationGraph* cst = CreateGraph(apNodeArray, aNodeCount, apInputPlugArray, aInputPlugCount, apOutputPlugArray, aOutputPlugCount, connectionCount, apConstantArray, aConstantCount, arAlloc);

		ConnectEdges(cst);		

		return cst;
	}

	static void Connect(Edge const& arEdges, EvaluationGraph& arGraph)
	{
		GraphPlug *source = 0, *destination = 0;
		
		uint32_t i, j;
		for(i=0;i < arGraph.m_NodeCount && (source == 0 || destination == 0);i++)
		{
			Node* node = arGraph.m_NodeArray[i];
			for(j=0;j<node->GetPlugCount() && (source == 0 || destination == 0);j++)
			{
				if(node->GetPlug(j).m_PlugId == arEdges.m_SourceId)
					source = &node->GetPlug(j);
				if(node->GetPlug(j).m_PlugId == arEdges.m_DestinationId)
					destination = &node->GetPlug(j);
			}
		}

		for(j=0;j<arGraph.m_Input->GetPlugCount() && (source == 0 || destination == 0);j++)
		{
			if(arGraph.m_Input->GetPlug(j).m_PlugId == arEdges.m_SourceId)
				source = &arGraph.m_Input->GetPlug(j);
			if(arGraph.m_Input->GetPlug(j).m_PlugId == arEdges.m_DestinationId)
				destination = &arGraph.m_Input->GetPlug(j);
		}


		for(j=0;j<arGraph.m_Output->GetPlugCount() && (source == 0 || destination == 0);j++)
		{
			if(arGraph.m_Output->GetPlug(j).m_PlugId == arEdges.m_SourceId)
				source = &arGraph.m_Output->GetPlug(j);
			if(arGraph.m_Output->GetPlug(j).m_PlugId == arEdges.m_DestinationId)
				destination = &arGraph.m_Output->GetPlug(j);
		}

		if(source && destination)
		{
			destination->m_Source = source;
		}
	}

	static void Connect(ConstantEdge const& arConstantEdge, Constant& arConstant, EvaluationGraph& arGraph)
	{
		GraphPlug *plug = 0;
		
		uint32_t i, j;
		for(i=0;i < arGraph.m_NodeCount && plug == 0; i++)
		{
			Node* node = arGraph.m_NodeArray[i];
			for(j=0;j<node->GetPlugCount() && plug == 0; j++)
			{
				if(node->GetPlug(j).m_PlugId == arConstantEdge.m_Id)
					plug = &node->GetPlug(j);
			}
		}

		for(j=0;j<arGraph.m_Input->GetPlugCount() && plug == 0; j++)
		{
			if(arGraph.m_Input->GetPlug(j).m_PlugId == arConstantEdge.m_Id)
				plug = &arGraph.m_Input->GetPlug(j);
		}


		for(j=0;j<arGraph.m_Output->GetPlugCount() && plug == 0; j++)
		{
			if(arGraph.m_Output->GetPlug(j).m_PlugId == arConstantEdge.m_Id)
				plug = &arGraph.m_Output->GetPlug(j);
		}

		if(plug)
		{
			arConstant.m_Plug = plug;
		}
	}

	EvaluationGraph* CreateEvaluationGraph(Graph* apGraph, GraphFactory const& arFactory, memory::Allocator& arAlloc)
	{
		EvaluationGraph* cst = 0;
		uint32_t i;
		Node**      nodes = arAlloc.ConstructArray<Node*>(apGraph->m_VerticesCount);
		GraphPlug** inputPlug = arAlloc.ConstructArray<GraphPlug*>(apGraph->m_InEdgesCount);
		GraphPlug** outputPlug = arAlloc.ConstructArray<GraphPlug*>(apGraph->m_OutEdgesCount);
		Constant**  constants = arAlloc.ConstructArray<Constant*>(apGraph->m_ConstantCount);

		for(i=0;i<apGraph->m_VerticesCount;i++)
		{
			nodes[i] = arFactory.Create(static_cast<eNodeType>(apGraph->m_Vertices[i].m_Id), arAlloc);
			nodes[i]->mID = apGraph->m_Vertices[i].m_Binding;
		}

		for(i=0;i<apGraph->m_InEdgesCount;i++)
		{
			inputPlug[i] = arFactory.Create(static_cast<ePlugType>(apGraph->m_InEdges[i].m_Id), arAlloc);
			inputPlug[i]->m_ID = apGraph->m_InEdges[i].m_Binding;
			inputPlug[i]->m_Input = false;
		}

		for(i=0;i<apGraph->m_OutEdgesCount;i++)
		{
			outputPlug[i] = arFactory.Create(static_cast<ePlugType>(apGraph->m_OutEdges[i].m_Id), arAlloc);
			outputPlug[i]->m_ID = apGraph->m_OutEdges[i].m_Binding;
			outputPlug[i]->m_Input = true;
		}

		for(i=0;i<apGraph->m_ConstantCount;i++)
		{
			constants[i] = arAlloc.Construct<Constant>();

			constants[i]->m_FloatValue = apGraph->m_ConstantEdges[i].m_FloatValue;
			constants[i]->m_VectorValue = apGraph->m_ConstantEdges[i].m_VectorValue;
		}

		cst = CreateGraph(nodes, apGraph->m_VerticesCount, inputPlug, apGraph->m_InEdgesCount, outputPlug, apGraph->m_OutEdgesCount, apGraph->m_EdgesCount, constants, apGraph->m_ConstantCount, arAlloc);

		for(i=0;i<apGraph->m_EdgesCount;i++)
		{
			cst->m_Graph.m_Edges[i] = apGraph->m_Edges[i];
			Connect(apGraph->m_Edges[i], *cst);			
		}

		for(i=0;i<apGraph->m_ConstantCount;i++)
		{
			cst->m_Graph.m_ConstantEdges[i].m_Id = apGraph->m_ConstantEdges[i].m_Id;
			Connect(apGraph->m_ConstantEdges[i], *cst->m_ConstantArray[i], *cst);
		}

		arAlloc.Deallocate(nodes);
		arAlloc.Deallocate(inputPlug);
		arAlloc.Deallocate(outputPlug);		
		arAlloc.Deallocate(constants);

		return cst;
	}

	void DestroyEvaluationGraph(EvaluationGraph* apGraph, memory::Allocator& arAlloc)
	{
		if(apGraph)
		{
			uint32_t i;
			for(i=0;i<apGraph->m_NodeCount;i++)
			{
				arAlloc.Deallocate(apGraph->m_NodeArray[i]);
			}
			arAlloc.Deallocate(apGraph->m_NodeArray);

			for(i=0;i<apGraph->m_Graph.m_InEdgesCount;i++)
			{
				arAlloc.Deallocate(apGraph->m_Input->mPlugArray[i]);
			}
			arAlloc.Deallocate(apGraph->m_Input->mPlugArray);
			arAlloc.Deallocate(apGraph->m_Input);

			for(i=0;i<apGraph->m_Graph.m_OutEdgesCount;i++)
			{
				arAlloc.Deallocate(apGraph->m_Output->mPlugArray[i]);
			}
			arAlloc.Deallocate(apGraph->m_Output->mPlugArray);
			arAlloc.Deallocate(apGraph->m_Output);

			for(i=0;i<apGraph->m_ConstantCount;i++)
			{
				arAlloc.Deallocate(apGraph->m_ConstantArray[i]);
			}
			arAlloc.Deallocate(apGraph->m_ConstantArray);

			arAlloc.Deallocate(apGraph->m_Graph.m_Vertices);
			arAlloc.Deallocate(apGraph->m_Graph.m_Edges);
			arAlloc.Deallocate(apGraph->m_Graph.m_InEdges);
			arAlloc.Deallocate(apGraph->m_Graph.m_OutEdges);
			arAlloc.Deallocate(apGraph->m_Graph.m_ConstantEdges);			

			arAlloc.Deallocate(apGraph);
		}
	}

	EvaluationGraphWorkspace*	CreateEvaluationGraphWorkspace(EvaluationGraph* apGraph, memory::Allocator& arAlloc)
	{		
		std::size_t sizePlugDataBlock = 0;
		uint32_t i, plugCount = 0;		

		for(i=0;i<apGraph->m_NodeCount;i++)
		{
			Node* node = apGraph->m_NodeArray[i];
			uint32_t j;
			for(j=0;j<node->GetPlugCount();j++)
			{
				plugCount++;
				sizePlugDataBlock = arAlloc.AlignAddress(sizePlugDataBlock, node->GetPlug(j).ValueAlign());
				sizePlugDataBlock += node->GetPlug(j).ValueSize();				
			}			
		}

		for(i=0;i<apGraph->m_Input->GetPlugCount();i++)
		{
			plugCount++;
			sizePlugDataBlock = arAlloc.AlignAddress(sizePlugDataBlock, apGraph->m_Input->GetPlug(i).ValueAlign());
			sizePlugDataBlock += apGraph->m_Input->GetPlug(i).ValueSize();
		}

		for(i=0;i<apGraph->m_Output->GetPlugCount();i++)
		{
			plugCount++;
			sizePlugDataBlock = arAlloc.AlignAddress(sizePlugDataBlock, apGraph->m_Output->GetPlug(i).ValueAlign());
			sizePlugDataBlock += apGraph->m_Output->GetPlug(i).ValueSize();
		}

		EvaluationGraphWorkspace* ws = arAlloc.Construct<EvaluationGraphWorkspace>();

		ws->m_EvaluationInfo.m_DataBlock.m_PlugCount = plugCount;
		ws->m_EvaluationInfo.m_DataBlock.m_EvaluationId = arAlloc.ConstructArray<uint32_t>(plugCount);
		
		ws->m_EvaluationInfo.m_DataBlock.m_Buffer = reinterpret_cast<char*>(arAlloc.Allocate( sizePlugDataBlock, ALIGN4F) );
		ws->m_EvaluationInfo.m_EvaluationId = 0;

		memset(ws->m_EvaluationInfo.m_DataBlock.m_EvaluationId, numeric_limits<uint32_t>::max_value, sizeof(mecanim::uint32_t)*plugCount);
		memset(ws->m_EvaluationInfo.m_DataBlock.m_Buffer, 0, sizePlugDataBlock);		

		for(i=0;i<apGraph->m_ConstantCount;i++)
		{
			if(apGraph->m_ConstantArray[i]->m_Plug)
			{
				ePlugType plugType = GetPlugType(*apGraph->m_ConstantArray[i]->m_Plug);
				switch(plugType)
				{
				case Float4Id:
					ws->m_EvaluationInfo.m_DataBlock.SetData(apGraph->m_ConstantArray[i]->m_VectorValue, apGraph->m_ConstantArray[i]->m_Plug->m_PlugId, apGraph->m_ConstantArray[i]->m_Plug->m_Offset, 0);
					break;
				case Float1Id:
					ws->m_EvaluationInfo.m_DataBlock.SetData(apGraph->m_ConstantArray[i]->m_VectorValue, apGraph->m_ConstantArray[i]->m_Plug->m_PlugId, apGraph->m_ConstantArray[i]->m_Plug->m_Offset, 0);
					break;
				case FloatId:
					ws->m_EvaluationInfo.m_DataBlock.SetData(apGraph->m_ConstantArray[i]->m_FloatValue, apGraph->m_ConstantArray[i]->m_Plug->m_PlugId, apGraph->m_ConstantArray[i]->m_Plug->m_Offset, 0);
					break;
				case UInt32Id:
					ws->m_EvaluationInfo.m_DataBlock.SetData(apGraph->m_ConstantArray[i]->m_UIntValue, apGraph->m_ConstantArray[i]->m_Plug->m_PlugId, apGraph->m_ConstantArray[i]->m_Plug->m_Offset, 0);
					break;
				case Int32Id:
					ws->m_EvaluationInfo.m_DataBlock.SetData(apGraph->m_ConstantArray[i]->m_IntValue, apGraph->m_ConstantArray[i]->m_Plug->m_PlugId, apGraph->m_ConstantArray[i]->m_Plug->m_Offset, 0);
					break;
				case BoolId:
					ws->m_EvaluationInfo.m_DataBlock.SetData(apGraph->m_ConstantArray[i]->m_BoolValue, apGraph->m_ConstantArray[i]->m_Plug->m_PlugId, apGraph->m_ConstantArray[i]->m_Plug->m_Offset, 0);
					break;
				}
			}			
		}

		return ws;
	}

	void DestroyEvaluationGraphWorkspace(EvaluationGraphWorkspace* apGraphWorkspace, memory::Allocator& arAlloc)
	{
		if(apGraphWorkspace)
		{
			arAlloc.Deallocate(apGraphWorkspace->m_EvaluationInfo.m_DataBlock.m_Buffer);
			arAlloc.Deallocate(apGraphWorkspace->m_EvaluationInfo.m_DataBlock.m_EvaluationId);
			arAlloc.Deallocate(apGraphWorkspace);
		}
	}
}

}