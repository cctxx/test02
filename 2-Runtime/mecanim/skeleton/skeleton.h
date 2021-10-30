#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/Math/Simd/xform.h"
#include "Runtime/mecanim/math/axes.h"


#include "Runtime/Serialize/Blobification/offsetptr.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Animation/MecanimArraySerialization.h"

namespace mecanim
{
namespace skeleton
{
	struct Node
	{
		DEFINE_GET_TYPESTRING(Node)

		Node() : m_ParentId(-1), m_AxesId(-1) {}
		int32_t m_ParentId;
		int32_t m_AxesId;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_ParentId);
			TRANSFER(m_AxesId);
		}	
	};

	struct Skeleton
	{
		DEFINE_GET_TYPESTRING(Skeleton)

		Skeleton() : m_Count(0), m_AxesCount(0) {}
		
		uint32_t			m_Count;
		OffsetPtr<Node>		m_Node;
		OffsetPtr<uint32_t>	m_ID;		// CRC(path)

		uint32_t				m_AxesCount;		
		OffsetPtr<math::Axes>	m_AxesArray;
		
		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_Count);
			MANUAL_ARRAY_TRANSFER2(Node, m_Node, m_Count);
			MANUAL_ARRAY_TRANSFER2(uint32_t, m_ID, m_Count);
			TRANSFER_BLOB_ONLY(m_AxesCount);
			MANUAL_ARRAY_TRANSFER2(math::Axes, m_AxesArray, m_AxesCount);
		}	
	};
	
	struct ATTRIBUTE_ALIGN(ALIGN4F) SkeletonPose
	{
		DEFINE_GET_TYPESTRING(SkeletonPose)

		SkeletonPose() : m_Count(0) {}

		uint32_t				m_Count;
		OffsetPtr<math::xform>	m_X;		

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_Count);
			MANUAL_ARRAY_TRANSFER2(math::xform, m_X, m_Count);
		}	
	};

	struct SkeletonMaskElement
	{
		DEFINE_GET_TYPESTRING(SkeletonMaskElement)

		SkeletonMaskElement(): m_PathHash(0), m_Weight(0.f){}

		uint32_t	m_PathHash;
		float		m_Weight;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_PathHash);
			TRANSFER(m_Weight);
		}	
	};

	struct SkeletonMask
	{
		DEFINE_GET_TYPESTRING(SkeletonMask)

		SkeletonMask():m_Count(0){}

		uint32_t						m_Count;
		OffsetPtr<SkeletonMaskElement>	m_Data;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_Count);
			MANUAL_ARRAY_TRANSFER2(SkeletonMaskElement, m_Data, m_Count);
		}
	};

	Skeleton*     CreateSkeleton(int32_t aNodeCount, int32_t aAxesCount, memory::Allocator& arAlloc);
	void DestroySkeleton(Skeleton* apSkeleton, memory::Allocator& arAlloc);

	SkeletonPose* CreateSkeletonPose(Skeleton const* apSkeleton, memory::Allocator& arAlloc);
	void DestroySkeletonPose(SkeletonPose* apSkeletonPose, memory::Allocator& arAlloc);

	SkeletonMask* CreateSkeletonMask(uint32_t aNodeCount, SkeletonMaskElement* elements, memory::Allocator& arAlloc);
	void DestroySkeletonMask(SkeletonMask* skeletonMask, memory::Allocator& arAlloc);

	// copy skeleton
	void SkeletonCopy(Skeleton const* apSrc, Skeleton* apDst);

	// copy pose
	void SkeletonPoseCopy(SkeletonPose const* apSrcPose, SkeletonPose* apDstPose);
	void SkeletonPoseCopy(SkeletonPose const* apSrcPose, SkeletonPose* apDstPose, uint32_t aIndexCount, int32_t const *apIndexArray);

	// Find & Copy pose based on name binding
	int32_t SkeletonFindNode(Skeleton const *apSkeleton, uint32_t aID);
	void SkeletonBuildIndexArray(int32_t *indexArray,Skeleton const* apSrcSkeleton,Skeleton const* apDstSkeleton);
	void SkeletonBuildReverseIndexArray(int32_t *reverseIndexArray,int32_t const*indexArray,Skeleton const* apSrcSkeleton,Skeleton const* apDstSkeleton);
	void SkeletonPoseCopy(Skeleton const* apSrcSkeleton, SkeletonPose const* apSrcPose, Skeleton const* apDstSkeleton, SkeletonPose* apDstPose);
	
	// set mask for a skeleton mask array
	void SkeletonPoseSetDirty(Skeleton const* apSkeleton, uint32_t* apSkeletonPoseMask, int aIndex, int aStopIndex, uint32_t aMask);

	// those functions work in place
	// computes a global pose from a local pose
	void SkeletonPoseComputeGlobal(Skeleton const* apSkeleton, SkeletonPose const* apLocalPose, SkeletonPose* apGlobalPose);	
	// computes a global pose from a local pose for part of the skeleton starting at aIndex (child) to aStopIndex (ancestor)
	void SkeletonPoseComputeGlobal(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseLocal, SkeletonPose* apSkeletonPoseGlobal, int aIndex, int aStopIndex);
	// computes a local pose from a global pose
	void SkeletonPoseComputeLocal(Skeleton const* apSkeleton, SkeletonPose const* apGlobalPose, SkeletonPose* apLocalPose);
	// computes a local pose from a global pose for part of the skeleton starting at aIndex (child) to aStopIndex (ancestor)
	void SkeletonPoseComputeLocal(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseGlobal, SkeletonPose* apSkeletonPoseLocal, int aIndex, int aStopIndex);
	// computes a global Q pose from a local Q pose
	void SkeletonPoseComputeGlobalQ(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseLocal, SkeletonPose* apSkeletonPoseGlobal);
	// computes a global Q pose from a local Q pose for part of the skeleton starting at aIndex (child) to aStopIndex (ancestor)
	void SkeletonPoseComputeGlobalQ(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseLocal, SkeletonPose* apSkeletonPoseGlobal, int aIndex, int aStopIndex);
	// computes a local Q pose from a global Q pose
	void SkeletonPoseComputeLocalQ(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseGlobal, SkeletonPose* apSkeletonPoseLocal);
	// computes a local Q pose from a global Q pose for part of the skeleton starting at aIndex (child) to aStopIndex (ancestor)
	void SkeletonPoseComputeLocalQ(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseGlobal, SkeletonPose* apSkeletonPoseLocal, int aIndex, int aStopIndex);
	
	// get dof for bone index in pose
	math::float4 SkeletonGetDoF(Skeleton const* apSkeleton,SkeletonPose const *apSkeletonPose, int32_t aIndex);
	// set dof for bone index in pose
	void SkeletonSetDoF(Skeleton const* apSkeleton, SkeletonPose * apSkeletonPose, math::float4 const& aDoF, int32_t aIndex);
	// algin x axis of skeleton node quaternion to ref node quaternion
	void SkeletonAlign(skeleton::Skeleton const *apSkeleton, math::float4 const &arRefQ, math::float4 & arQ, int32_t aIndex);
	// algin x axis of skeleton pose node to ref pose node
	void SkeletonAlign(skeleton::Skeleton const *apSkeleton, skeleton::SkeletonPose const*apSkeletonPoseRef, skeleton::SkeletonPose *apSkeletonPose, int32_t aIndex);
	
	// ik
	// compute end point of a node which is x * xcos * lenght.
	math::float4 SkeletonNodeEndPoint(Skeleton const *apSkeleton, int32_t aIndex, SkeletonPose const*apSkeletonPose);
	// The apSkeletonPoseWorkspace parameter has to be a valid global pose, otherwise unexpected result may occur
	void Skeleton2BoneAdjustLength(Skeleton const *apSkeleton, int32_t aIndexA, int32_t aIndexB, int32_t aIndexC, math::float4 const &aTarget, math::float1 const& aRatio, SkeletonPose *apSkeletonPose, SkeletonPose *apSkeletonPoseWorkspace); 
	// The apSkeletonPoseWorkspace parameter has to be a valid global pose, otherwise unexpected result may occur
	void Skeleton2BoneIK(Skeleton const *apSkeleton, int32_t aIndexA, int32_t aIndexB, int32_t aIndexC, math::float4 const &aTarget, float aWeight, SkeletonPose *apSkeletonPose, SkeletonPose *apSkeletonPoseWorkspace);
	// The apSkeletonPoseWorkspace parameter has to be a valid global pose, otherwise unexpected result may occur
	void Skeleton3BoneIK(Skeleton const *apSkeleton, int32_t aIndexA, int32_t aIndexB, int32_t aIndexC, math::float4 const &aTarget, float weight, SkeletonPose *apSkeletonPose, SkeletonPose *apSkeletonPoseWorkspace); 

	// setup axes utilities
	struct SetupAxesInfo
	{
		float m_PreQ[4];
		float m_MainAxis[4];
		float m_Min[4];
		float m_Max[4];
		float m_Sgn[4];
		math::AxesType m_Type;
		int32_t m_ForceAxis;
	};

	void SetupAxes(skeleton::Skeleton *apSkeleton, skeleton::SkeletonPose const *apSkeletonPoseGlobal, SetupAxesInfo const& apSetupAxesInfo, int32_t aIndex, int32_t aAxisIndex, bool aLeft, float aLen = 1.0f);
}

}
