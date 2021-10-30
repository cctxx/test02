#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/animation/curvedata.h"
#include "Runtime/mecanim/human/human.h"
#include "Runtime/mecanim/human/hand.h"
#include "Runtime/Math/Vector2.h"

#include "Runtime/Serialize/Blobification/offsetptr.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Animation/MecanimArraySerialization.h" 

namespace mecanim
{

namespace animation
{
	struct MotionNeighborList
	{
		DEFINE_GET_TYPESTRING(MotionNeighborList)
		
		MotionNeighborList() : m_Count(0)
		{
		}
		
		uint32_t m_Count;
		OffsetPtr<uint32_t> m_NeighborArray;
		
		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_Count);
			MANUAL_ARRAY_TRANSFER2(uint32_t, m_NeighborArray, m_Count);
		}
	};
	
	// Constant data for 1D blend node types - thresholds
	struct Blend1dDataConstant
	{
		DEFINE_GET_TYPESTRING(Blend2dDataConstant)
		
		Blend1dDataConstant() : m_ChildCount(0)
		{
		}
		
		uint32_t			m_ChildCount;
		OffsetPtr<float>	m_ChildThresholdArray;
		
		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_ChildCount);
			MANUAL_ARRAY_TRANSFER2(float, m_ChildThresholdArray, m_ChildCount);
		}
	};
	
	// Constant data for 2D blend node types - positions plus precomputed data to speed up blending
	struct Blend2dDataConstant
	{
		DEFINE_GET_TYPESTRING(Blend2dDataConstant)
		
		Blend2dDataConstant() : m_ChildCount(0), m_ChildMagnitudeCount(0), m_ChildPairVectorCount(0), m_ChildPairAvgMagInvCount(0), m_ChildNeighborListCount(0)
		{
		}
		
		uint32_t				m_ChildCount;
		OffsetPtr<Vector2f>		m_ChildPositionArray;

		uint32_t				m_ChildMagnitudeCount;
		OffsetPtr<float>		m_ChildMagnitudeArray; // Used by type 2
		uint32_t				m_ChildPairVectorCount;
		OffsetPtr<Vector2f>		m_ChildPairVectorArray; // Used by type 2, (3 TODO)
		uint32_t				m_ChildPairAvgMagInvCount;
		OffsetPtr<float>		m_ChildPairAvgMagInvArray; // Used by type 2
		uint32_t						m_ChildNeighborListCount;
		OffsetPtr<MotionNeighborList>	m_ChildNeighborListArray; // Used by type 2, (3 TODO)
		
		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_ChildCount);
			MANUAL_ARRAY_TRANSFER2(Vector2f, m_ChildPositionArray, m_ChildCount);
			
			TRANSFER_BLOB_ONLY(m_ChildMagnitudeCount);
			MANUAL_ARRAY_TRANSFER2(float, m_ChildMagnitudeArray, m_ChildMagnitudeCount);
			TRANSFER_BLOB_ONLY(m_ChildPairVectorCount);
			MANUAL_ARRAY_TRANSFER2(Vector2f, m_ChildPairVectorArray, m_ChildPairVectorCount);
			TRANSFER_BLOB_ONLY(m_ChildPairAvgMagInvCount);
			MANUAL_ARRAY_TRANSFER2(float, m_ChildPairAvgMagInvArray, m_ChildPairAvgMagInvCount);
			TRANSFER_BLOB_ONLY(m_ChildNeighborListCount);
			MANUAL_ARRAY_TRANSFER2(MotionNeighborList, m_ChildNeighborListArray, m_ChildNeighborListCount);
		}
	};

	struct BlendTreeNodeConstant
	{
		DEFINE_GET_TYPESTRING(BlendTreeNodeConstant)
		
		BlendTreeNodeConstant(): m_BlendType(0), m_BlendEventID(-1), m_BlendEventYID(-1), m_ChildCount(0), m_ClipID(-1), m_Duration(0), m_CycleOffset(0), m_Mirror(false)
		{
			
		}

		uint32_t				m_BlendType;

		uint32_t				m_BlendEventID;
		uint32_t				m_BlendEventYID;
		uint32_t				m_ChildCount;
		OffsetPtr<uint32_t>		m_ChildIndices;
		
		OffsetPtr<Blend1dDataConstant>	m_Blend1dData;
		OffsetPtr<Blend2dDataConstant>	m_Blend2dData;

		uint32_t				m_ClipID; // assert( m_ClipID != -1 && mClipBlendCount == 0)
		float					m_Duration;
		float					m_CycleOffset;
		bool					m_Mirror;

		// Unity 4.1 introduced 2D blendtrees. The data layout has been changed there.
		template<class TransferFunction>
		inline void Transfer_4_0_BackwardsCompatibility (TransferFunction& transfer)
		{
			if (transfer.IsOldVersion(1))
			{
				if (m_Blend1dData.IsNull())
				{	
					mecanim::memory::ChainedAllocator* allocator = static_cast<mecanim::memory::ChainedAllocator*> (transfer.GetUserData());
					m_Blend1dData = allocator->Construct<Blend1dDataConstant>();
				}

				OffsetPtr<float>& m_ChildThresholdArray = m_Blend1dData->m_ChildThresholdArray;	
				MANUAL_ARRAY_TRANSFER2(float, m_ChildThresholdArray, m_Blend1dData->m_ChildCount);
			}
		}

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			transfer.SetVersion(2);
			
			TRANSFER(m_BlendType);
			TRANSFER(m_BlendEventID);
			TRANSFER(m_BlendEventYID);
			
			TRANSFER_BLOB_ONLY(m_ChildCount);
			MANUAL_ARRAY_TRANSFER2(uint32_t, m_ChildIndices, m_ChildCount);
			
			TRANSFER(m_Blend1dData);
			TRANSFER(m_Blend2dData);

			TRANSFER(m_ClipID);
			TRANSFER(m_Duration);			

			TRANSFER(m_CycleOffset);			
			TRANSFER(m_Mirror);			
			transfer.Align();
			
			Transfer_4_0_BackwardsCompatibility(transfer);
			
		}
	};

	struct BlendTreeConstant
	{
		DEFINE_GET_TYPESTRING(BlendTreeConstant)

		BlendTreeConstant () :m_NodeCount(0)
		{
		}
		
		uint32_t										m_NodeCount;
		OffsetPtr< OffsetPtr<BlendTreeNodeConstant> >	m_NodeArray;		
		
		OffsetPtr<ValueArrayConstant>					m_BlendEventArrayConstant;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_NodeCount);
			MANUAL_ARRAY_TRANSFER2( OffsetPtr<mecanim::animation::BlendTreeNodeConstant>, m_NodeArray, m_NodeCount);

			TRANSFER(m_BlendEventArrayConstant);
		}
	};
	
	struct BlendTreeMemory
	{
		DEFINE_GET_TYPESTRING(BlendTreeMemory)

		BlendTreeMemory() : m_NodeCount(0) {}

		uint32_t			m_NodeCount;
		OffsetPtr<float>	m_NodeDurationArray;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_NodeCount);
			MANUAL_ARRAY_TRANSFER2(float, m_NodeDurationArray, m_NodeCount);
		}
	};

	struct BlendTreeInput
	{
		BlendTreeInput() : m_BlendValueArray(0)
		{
		}

		ValueArray*				m_BlendValueArray;		
	};

	struct BlendTreeNodeOutput
	{
		BlendTreeNodeOutput() : m_BlendValue(0), m_ID(0), m_Reverse(false), m_Mirror(false), m_CycleOffset(0)
		{

		}
		
		float			m_BlendValue;
		uint32_t		m_ID;
		bool			m_Reverse;
		bool			m_Mirror;
		float			m_CycleOffset;
	};

	struct BlendTreeOutput
	{
		BlendTreeOutput() :  m_Duration(0),
							 m_MaxBlendedClip(0)
		{}

		BlendTreeNodeOutput*	m_OutputBlendArray;		
		uint32_t				m_MaxBlendedClip;
		float					m_Duration;
	};

	struct BlendTreeWorkspace
	{
		BlendTreeWorkspace() : m_BlendArray(0), m_TempWeightArray(0), m_TempCropArray(0), m_ChildInputVectorArray(0)
		{

		}


		float* m_BlendArray;
		float* m_TempWeightArray;
		int* m_TempCropArray;
		Vector2f* m_ChildInputVectorArray;
	};
	
	void GetWeights (const BlendTreeNodeConstant& nodeConstant, BlendTreeWorkspace &workspace, float* weightArray, float blendValueX, float blendValueY);

	// Overload for creating 1D blend node
	BlendTreeNodeConstant* CreateBlendTreeNodeConstant(uint32_t blendValueID, uint32_t childCount, uint32_t* childIndices, float* blendTreeThresholdArray,  memory::Allocator& alloc);
	// Overload for creating 2D blend node
	BlendTreeNodeConstant* CreateBlendTreeNodeConstant(uint32_t blendValueID, uint32_t blendValueYID, int blendType, uint32_t childCount, uint32_t* childIndices, Vector2f* blendTreePositionArray,  memory::Allocator& alloc);
	// Overload for creating leaf blend node
	BlendTreeNodeConstant* CreateBlendTreeNodeConstant(uint32_t clipID, float duration, bool mirror, float cycle, memory::Allocator& alloc);

	BlendTreeConstant* CreateBlendTreeConstant(BlendTreeNodeConstant** nodeArray, uint32_t nodeCount, memory::Allocator& alloc);	
	BlendTreeConstant* CreateBlendTreeConstant(uint32_t clipID, memory::Allocator& alloc);		
	void DestroyBlendTreeConstant(BlendTreeConstant * constant, memory::Allocator& alloc);

	BlendTreeMemory* CreateBlendTreeMemory(BlendTreeConstant const* constant, memory::Allocator& alloc);
	void DestroyBlendTreeMemory(BlendTreeMemory *memory, memory::Allocator& alloc);

	BlendTreeInput* CreateBlendTreeInput(BlendTreeConstant const* constant, memory::Allocator& alloc);
	void DestroyBlendTreeInput(BlendTreeInput * input, memory::Allocator& alloc);

	BlendTreeOutput* CreateBlendTreeOutput(BlendTreeConstant const* constant, uint32_t maxBlendedClip, memory::Allocator& alloc);
	void DestroyBlendTreeOutput(BlendTreeOutput * output, memory::Allocator& alloc);

	BlendTreeWorkspace* CreateBlendTreeWorkspace(BlendTreeConstant const* constant, memory::Allocator& alloc);
	void DestroyBlendTreeWorkspace(BlendTreeWorkspace * workspace, memory::Allocator& alloc);


	void EvaluateBlendTree(const BlendTreeConstant& constant, const BlendTreeInput &input, const BlendTreeMemory &memory, BlendTreeOutput &output, BlendTreeWorkspace &workspace);

	mecanim::uint32_t GetLeafCount(const BlendTreeConstant& constant);
	void FillLeafIDArray(const BlendTreeConstant& constant, uint32_t* leafIDArray);
	mecanim::uint32_t GetMaxBlendCount(const BlendTreeConstant& constant);
}
}
