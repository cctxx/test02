#include "UnityPrefix.h"
#include "Runtime/mecanim/skeleton/skeleton.h"

#include "Runtime/mecanim/graph/plugbinder.h"
#include "Runtime/mecanim/animation/common.h"
#include "Runtime/mecanim/animation/poseblender.h"

namespace mecanim
{

namespace animation
{
	namespace 
	{		
		void SetPoseBlenderValue(graph::GraphPlug const* plug, uint32_t index, PoseBlenderInput const* input, graph::EvaluationGraphWorkspace* graphMemory)
		{
			float value;
			plug->ReadData(&value, graphMemory->m_EvaluationInfo);   
			input->m_WeightArray[index] += value;			
		}
	}

	PoseBlenderConstant* CreatePoseBlenderConstant(skeleton::Skeleton * skeleton, skeleton::SkeletonPose*	defaultPose, skeleton::SkeletonPose**	arrayPose, uint32_t count, graph::Graph* graph, memory::Allocator& alloc)
	{
		PoseBlenderConstant *cst = alloc.Construct<PoseBlenderConstant>();

		cst->m_Skeleton = skeleton;
		cst->m_SkeletonPoseCount = count;

		cst->m_DefaultSkeletonPose = defaultPose;
		cst->m_SkeletonPoseArray = alloc.ConstructArray<skeleton::SkeletonPose*>(count);
		cst->m_PosesID = alloc.ConstructArray<uint32_t>(count);
		cst->m_HasGraph = graph != 0;
		cst->m_Graph = graph;

		uint32_t i;
		for(i=0;i<count;i++)
		{
			cst->m_SkeletonPoseArray[i] = arrayPose[i];
		}

		if(cst->m_HasGraph)
		{
			cst->m_ValuesConstant = CreateValueArrayConstant(kFloatType, cst->m_Graph->m_InEdgesCount, alloc);
			for(i=0;i<cst->m_Graph->m_InEdgesCount;++i)
			{
				cst->m_ValuesConstant->m_ValueArray[i].m_ID = cst->m_Graph->m_InEdges[i].m_Binding;
			}
		}

		return cst;
	}

	void DestroyPoseBlenderConstant(PoseBlenderConstant * constant, memory::Allocator& alloc)
	{
		if(constant)
		{
			alloc.Deallocate(constant->m_PosesID);
			alloc.Deallocate(constant->m_SkeletonPoseArray);
			alloc.Deallocate(constant);
		}
	}

	void InitializePoseBlenderConstant(PoseBlenderConstant * constant, graph::GraphFactory const& factory, memory::Allocator& alloc)
	{
		if(constant->m_Graph)
		{
			constant->m_EvaluationGraph = graph::CreateEvaluationGraph(constant->m_Graph, factory, alloc);
		}
	}

	void ClearPoseBlenderConstant(PoseBlenderConstant * constant, memory::Allocator& alloc)
	{
		if(constant->m_EvaluationGraph)
		{
			graph::DestroyEvaluationGraph(constant->m_EvaluationGraph, alloc);
			constant->m_EvaluationGraph = 0;
		}
	}

	PoseBlenderInput* CreatePoseBlenderInput(PoseBlenderConstant const* constant, memory::Allocator& alloc)
	{
		PoseBlenderInput *in = alloc.Construct<PoseBlenderInput>();

		if(constant->m_ValuesConstant)
		{
			in->m_Values = CreateValueArray(constant->m_ValuesConstant, alloc);
		}
		in->m_WeightPoseCount = constant->m_SkeletonPoseCount;
		in->m_SkeletonPose = skeleton::CreateSkeletonPose(constant->m_Skeleton, alloc);
		in->m_WeightArray = alloc.ConstructArray<float>(constant->m_SkeletonPoseCount);

		uint32_t i;
		for(i=0;i<in->m_WeightPoseCount;i++)
		{
			in->m_WeightArray[i] = 0.f;
		}
		
		return in;
	}

	void DestroyPoseBlenderInput(PoseBlenderInput * input, memory::Allocator& alloc)
	{
		if(input)
		{
			skeleton::DestroySkeletonPose(input->m_SkeletonPose, alloc);
			alloc.Deallocate(input->m_WeightArray);
			alloc.Deallocate(input);
		}
	}

	PoseBlenderWorkspace* CreatePoseBlenderWorkspace(PoseBlenderConstant const* constant, memory::Allocator& alloc)
	{
		PoseBlenderWorkspace *ws = alloc.Construct<PoseBlenderWorkspace>();
		ws->m_SkeletonPose = skeleton::CreateSkeletonPose(constant->m_Skeleton, alloc);

		return ws;
	}
	void DestroyPoseBlenderWorkspace(PoseBlenderWorkspace * workspace, memory::Allocator& alloc)
	{
		if(workspace)
		{
			skeleton::DestroySkeletonPose(workspace->m_SkeletonPose, alloc);
			alloc.Deallocate(workspace);
		}
	}

	PoseBlenderMemory* CreatePoseBlenderMemory(PoseBlenderConstant const* constant, memory::Allocator& alloc)
	{
		PoseBlenderMemory* mem = alloc.Construct<PoseBlenderMemory>();

		if(constant->m_EvaluationGraph)
		{
			mem->m_GraphWS = graph::CreateEvaluationGraphWorkspace(constant->m_EvaluationGraph, alloc);
			mem->m_InputPoseBlenderBindingCount = 0;
			uint32_t i,j;
			for(i=0;i<constant->m_EvaluationGraph->m_Output->mPlugCount;i++)
			{
				graph::GraphPlug const* plug = constant->m_EvaluationGraph->m_Output->mPlugArray[i];
				for(j=0;j<constant->m_SkeletonPoseCount;j++)
				{
					if( plug->m_ID == constant->m_PosesID[j])
					{
						mem->m_InputPoseBlenderBindingCount++;
					}
				}
			}
			if(mem->m_InputPoseBlenderBindingCount)
			{
				mem->m_InputPoseBlenderBinding = alloc.ConstructArray<SetPoseBlenderInput2>(mem->m_InputPoseBlenderBindingCount);
				uint32_t k = 0;
				for(i=0;i<constant->m_EvaluationGraph->m_Output->mPlugCount;i++)
				{
					graph::GraphPlug const* plug = constant->m_EvaluationGraph->m_Output->mPlugArray[i];
					for(j=0;j<constant->m_SkeletonPoseCount;j++)
					{
						if( plug->m_ID == constant->m_PosesID[j])
						{
							mem->m_InputPoseBlenderBinding[k++] = bind( SetPoseBlenderValue, plug, j);
						}
					}
				}
			}
		}

		return mem;
	}

	void DestroyPoseBlenderMemory(PoseBlenderMemory* memory, memory::Allocator& alloc)
	{
		if(memory)
		{
			graph::DestroyEvaluationGraphWorkspace(memory->m_GraphWS, alloc);
			alloc.Deallocate(memory->m_InputPoseBlenderBinding);
			alloc.Deallocate(memory);
		}
	}


	PoseBlenderOutput* CreatePoseBlenderOutput(PoseBlenderConstant const* constant, memory::Allocator& alloc)
	{
		PoseBlenderOutput *out = alloc.Construct<PoseBlenderOutput>();
		out->m_SkeletonPose = skeleton::CreateSkeletonPose(constant->m_Skeleton, alloc);

		return out;
	}

	void DestroyPoseBlenderOutput(PoseBlenderOutput * output, memory::Allocator& alloc)
	{
		if(output)
		{
			skeleton::DestroySkeletonPose(output->m_SkeletonPose, alloc);
			alloc.Deallocate(output);
		}
	}

	void EvaluatePoseBlender(PoseBlenderConstant const * constant, PoseBlenderInput const * input, PoseBlenderOutput * output, PoseBlenderMemory* memory, PoseBlenderWorkspace * workspace)
	{		
		if(constant->m_EvaluationGraph)
		{
			memory->m_GraphWS->m_EvaluationInfo.m_EvaluationId++;

			// Value Array is constructed based on graph so they should be sync
			uint32_t i, count = constant->m_ValuesConstant->m_Count;
			for(i=0;i<count;i++)
			{
				SetPlugValue(&constant->m_EvaluationGraph->m_Input->GetPlug(i), i, input->m_Values, memory->m_GraphWS);
			}
			
			count = memory->m_InputPoseBlenderBindingCount;
			for(i=0;i<count;i++)
			{
				memory->m_InputPoseBlenderBinding[i](input, memory->m_GraphWS);			
			}
		}

		skeleton::SkeletonPoseCopy(input->m_SkeletonPose, output->m_SkeletonPose);
		uint32_t i;
		for(i = 0; i < constant->m_SkeletonPoseCount; i++)
		{
			skeleton::SkeletonPoseSub(constant->m_SkeletonPoseArray[i], input->m_SkeletonPose, workspace->m_SkeletonPose);
			skeleton::SkeletonPoseWeight(workspace->m_SkeletonPose, math::float1(input->m_WeightArray[i]), workspace->m_SkeletonPose);
			skeleton::SkeletonPoseAdd(output->m_SkeletonPose, workspace->m_SkeletonPose, output->m_SkeletonPose);
		}
	}	
}

}
