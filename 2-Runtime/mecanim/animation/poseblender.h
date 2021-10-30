#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/object.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/bind.h"

#include "Runtime/mecanim/generic/valuearray.h"
#include "Runtime/mecanim/graph/plug.h"
#include "Runtime/mecanim/graph/graph.h"

// forwards

namespace mecanim
{

namespace skeleton	{struct Skeleton; struct SkeletonPose;}

namespace animation
{
	// Constant 
	struct PoseBlenderConstant
	{
		PoseBlenderConstant()
			:m_Skeleton(0), 
			m_DefaultSkeletonPose(0), 
			m_SkeletonPoseArray(0), 
			m_PosesID(0),
			m_SkeletonPoseCount(0),
			m_Graph(0),
			m_ValuesConstant(0),
			m_HasGraph(false),
			m_EvaluationGraph(0)
		{
		}

		skeleton::Skeleton*			m_Skeleton;
		skeleton::SkeletonPose*		m_DefaultSkeletonPose;
		skeleton::SkeletonPose**	m_SkeletonPoseArray;
		uint32_t*					m_PosesID;					
		graph::Graph*				m_Graph;
		ValueArrayConstant const*	m_ValuesConstant;
		bool						m_HasGraph;
		
		uint32_t					m_SkeletonPoseCount;	

		graph::EvaluationGraph*		m_EvaluationGraph;
	};

	struct PoseBlenderInput
	{
		PoseBlenderInput(): m_Values(0), m_WeightArray(0), m_SkeletonPose(0), m_WeightPoseCount(0){}

		ValueArray*				m_Values;
		float*					m_WeightArray;
		skeleton::SkeletonPose*	m_SkeletonPose;
		uint32_t				m_WeightPoseCount;
	};

	struct PoseBlenderWorkspace
	{
		PoseBlenderWorkspace(): m_SkeletonPose(0){}
		skeleton::SkeletonPose*	m_SkeletonPose;
	};

	typedef binder2<function<void (graph::GraphPlug const*, uint32_t, PoseBlenderInput const*,	graph::EvaluationGraphWorkspace*)>::ptr,
					graph::GraphPlug const*,
					uint32_t> 
			SetPoseBlenderInput2;

	struct PoseBlenderMemory
	{
		PoseBlenderMemory() : m_InputPoseBlenderBindingCount(0), m_InputPoseBlenderBinding(0), m_GraphWS(0)
		{
		}
		
		uint32_t				m_InputPoseBlenderBindingCount;
		SetPoseBlenderInput2*	m_InputPoseBlenderBinding;

		graph::EvaluationGraphWorkspace*		m_GraphWS;
	};	


	struct PoseBlenderOutput : public Object
	{
		PoseBlenderOutput():m_SkeletonPose(0){}

		skeleton::SkeletonPose*	m_SkeletonPose;	
	};

	PoseBlenderConstant* CreatePoseBlenderConstant(skeleton::Skeleton * skeleton, skeleton::SkeletonPose* defaultPose, skeleton::SkeletonPose**	arrayPose, uint32_t count, graph::Graph* graph, memory::Allocator& alloc);
	void DestroyPoseBlenderConstant(PoseBlenderConstant * constant, memory::Allocator& alloc);

	void InitializePoseBlenderConstant(PoseBlenderConstant * constant, graph::GraphFactory const& factory, memory::Allocator& alloc);
	void ClearPoseBlenderConstant(PoseBlenderConstant * constant, memory::Allocator& alloc);	

	PoseBlenderInput* CreatePoseBlenderInput(PoseBlenderConstant const* constant, memory::Allocator& alloc);
	void DestroyPoseBlenderInput(PoseBlenderInput * input, memory::Allocator& alloc);

	PoseBlenderWorkspace* CreatePoseBlenderWorkspace(PoseBlenderConstant const* constant, memory::Allocator& alloc);
	void DestroyPoseBlenderWorkspace(PoseBlenderWorkspace * workspace, memory::Allocator& alloc);

	PoseBlenderMemory* CreatePoseBlenderMemory(PoseBlenderConstant const* constant, memory::Allocator& alloc);
	void DestroyPoseBlenderMemory(PoseBlenderMemory* amemory, memory::Allocator& alloc);

	PoseBlenderOutput* CreatePoseBlenderOutput(PoseBlenderConstant const* constant, memory::Allocator& alloc);
	void DestroyPoseBlenderOutput(PoseBlenderOutput * output, memory::Allocator& alloc);

	void EvaluatePoseBlender(PoseBlenderConstant const * constant, PoseBlenderInput const * input, PoseBlenderOutput * output, PoseBlenderMemory* memory, PoseBlenderWorkspace * workspace);
}

}
