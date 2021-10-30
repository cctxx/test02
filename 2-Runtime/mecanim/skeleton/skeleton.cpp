#include "UnityPrefix.h"
#include "Runtime/mecanim/skeleton/skeleton.h"

namespace mecanim
{

namespace skeleton
{
	Skeleton*     CreateSkeleton(int32_t aNodeCount, int32_t aAxesCount, memory::Allocator& arAlloc)
	{	
		Skeleton* skeleton = arAlloc.Construct<Skeleton>();

		skeleton->m_Count = aNodeCount;		
		skeleton->m_Node = arAlloc.ConstructArray<Node>(aNodeCount);
		skeleton->m_ID = arAlloc.ConstructArray<uint32_t>(aNodeCount);

		skeleton->m_AxesCount = aAxesCount; 
		if(skeleton->m_AxesCount)
		{
			skeleton->m_AxesArray = arAlloc.ConstructArray<math::Axes>(aAxesCount);
		}

		return skeleton;
	}	

	SkeletonPose* CreateSkeletonPose(Skeleton const* apSkeleton, memory::Allocator& arAlloc)
	{	
		SkeletonPose* skeletonPose = arAlloc.Construct<SkeletonPose>();

		skeletonPose->m_Count = apSkeleton->m_Count;
		skeletonPose->m_X = arAlloc.ConstructArray<math::xform>(apSkeleton->m_Count);

		return skeletonPose;
	}

	void DestroySkeleton(Skeleton* apSkeleton, memory::Allocator& arAlloc)
	{
		if(apSkeleton)
		{
			arAlloc.Deallocate(apSkeleton->m_Node);
			arAlloc.Deallocate(apSkeleton->m_ID);
			arAlloc.Deallocate(apSkeleton->m_AxesArray);

			arAlloc.Deallocate(apSkeleton);
		}
	}

	void DestroySkeletonPose(SkeletonPose* apSkeletonPose, memory::Allocator& arAlloc)
	{
		if(apSkeletonPose)
		{
			arAlloc.Deallocate(apSkeletonPose->m_X);
			arAlloc.Deallocate(apSkeletonPose);
		}
	}

	SkeletonMask* CreateSkeletonMask(uint32_t aNodeCount, SkeletonMaskElement* elements, memory::Allocator& arAlloc)
	{
		SkeletonMask* skeletonMask = arAlloc.Construct<SkeletonMask>();

		skeletonMask->m_Count = aNodeCount;
		skeletonMask->m_Data = arAlloc.ConstructArray<SkeletonMaskElement>(aNodeCount);

		memcpy(skeletonMask->m_Data.Get(), elements, sizeof(SkeletonMaskElement)*aNodeCount);

		return skeletonMask;
	}

	void DestroySkeletonMask(SkeletonMask* skeletonMask, memory::Allocator& arAlloc)
	{
		if(skeletonMask)
		{
			arAlloc.Deallocate(skeletonMask->m_Data);
			arAlloc.Deallocate(skeletonMask);
		}
	}

	void SkeletonCopy(Skeleton const* apSrc, Skeleton* apDst)
	{
		apDst->m_Count = apSrc->m_Count;
		for(int nodeIter = 0; nodeIter < apDst->m_Count; nodeIter++)
		{
			apDst->m_Node[nodeIter] = apSrc->m_Node[nodeIter];
			apDst->m_ID[nodeIter] = apSrc->m_ID[nodeIter];
		}

		apDst->m_AxesCount = apSrc->m_AxesCount;
		for(int axesIter = 0; axesIter < apDst->m_AxesCount; axesIter++)
		{
			apDst->m_AxesArray[axesIter] = apSrc->m_AxesArray[axesIter];
		}
	}

	void SkeletonPoseCopy(SkeletonPose const* apSrcPose, SkeletonPose* apDstPose)
	{		
		uint32_t count = math::minimum(apSrcPose->m_Count, apDstPose->m_Count);
		memcpy(&apDstPose->m_X[0], &apSrcPose->m_X[0], count*sizeof(math::xform));
	}

	void SkeletonPoseCopy(SkeletonPose const* apSrcPose, SkeletonPose* apDstPose, uint32_t aIndexCount, int32_t const *apIndexArray)
	{
		for(uint32_t i = 0 ; i < aIndexCount; i++)
		{
			int32_t j = apIndexArray[i];
			apDstPose->m_X[j] = apSrcPose->m_X[i];
		}
	}

	int32_t SkeletonFindNode(Skeleton const* apSkeleton, uint32_t aID )
	{
		int32_t ret = -1;

		int32_t i;
		for(i = 0; ret == -1 && i < apSkeleton->m_Count; i++)
		{
			if(apSkeleton->m_ID[i] == aID)
			{
				ret = i;
			}
		}

		return ret;
	}

	void SkeletonBuildIndexArray(int32_t *indexArray,Skeleton const* apSrcSkeleton,Skeleton const* apDstSkeleton)
	{
		for(uint32_t i = 0; i < apSrcSkeleton->m_Count; i++)
		{
			indexArray[i] = SkeletonFindNode(apDstSkeleton,apSrcSkeleton->m_ID[i]);
		}
	}

	void SkeletonBuildReverseIndexArray(int32_t *reverseIndexArray,int32_t const*indexArray,Skeleton const* apSrcSkeleton,Skeleton const* apDstSkeleton)
	{
		for(uint32_t dstIter = 0; dstIter < apDstSkeleton->m_Count; dstIter++)
		{
			reverseIndexArray[dstIter] = -1;
		}
		
		for(uint32_t srcIter = 0; srcIter < apSrcSkeleton->m_Count; srcIter++)
		{
			if(indexArray[srcIter] != -1)
			{
				reverseIndexArray[indexArray[srcIter]] = srcIter;
			}
		}
	}

	void SkeletonPoseCopy(Skeleton const* apSrcSkeleton, SkeletonPose const* apSrcPose, Skeleton const* apDstSkeleton, SkeletonPose* apDstPose)
	{
		uint32_t i;
		for(i = 0; i < apSrcSkeleton->m_Count; i++)
		{
			uint32_t j;
			for(j = 0; j < apDstSkeleton->m_Count; j++)
			{
				if( apSrcSkeleton->m_ID[i] == apDstSkeleton->m_ID[j] )
				{
					apDstPose->m_X[j] = apSrcPose->m_X[i];
					break;
				}
			}			
		}
	}

	void SkeletonPoseSetDirty(Skeleton const* apSkeleton, uint32_t* apSkeletonPoseMask, int aIndex, int aStopIndex, uint32_t aMask)
	{
		int parentIndex = apSkeleton->m_Node[aIndex].m_ParentId;

		if(parentIndex != -1)
		{
			if(aIndex != aStopIndex)
			{
				SkeletonPoseSetDirty(apSkeleton,apSkeletonPoseMask, parentIndex, aStopIndex, aMask);
			}
		}
		
		apSkeletonPoseMask[aIndex] = apSkeletonPoseMask[aIndex] | aMask;
	}	

	void SkeletonPoseComputeGlobal(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseLocal, SkeletonPose* apSkeletonPoseGlobal)
	{
		apSkeletonPoseGlobal->m_X[0] = apSkeletonPoseLocal->m_X[0];

		uint32_t i;
		for(i=1;i < apSkeleton->m_Count; i++)
		{
			apSkeletonPoseGlobal->m_X[i] = xformMul( apSkeletonPoseGlobal->m_X[apSkeleton->m_Node[i].m_ParentId], apSkeletonPoseLocal->m_X[i]);
		}
	}	

	void SkeletonPoseComputeLocal(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseGlobal, SkeletonPose* apSkeletonPoseLocal)
	{
		uint32_t i;
		for(i=apSkeleton->m_Count-1;i > 0; i--)
		{
			apSkeletonPoseLocal->m_X[i] = xformInvMul( apSkeletonPoseGlobal->m_X[apSkeleton->m_Node[i].m_ParentId], apSkeletonPoseGlobal->m_X[i]);
		}

		apSkeletonPoseLocal->m_X[0] = apSkeletonPoseGlobal->m_X[0];
	}

	void SkeletonPoseComputeGlobal(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseLocal, SkeletonPose* apSkeletonPoseGlobal, int aIndex, int aStopIndex)
	{
		int parentIndex = apSkeleton->m_Node[aIndex].m_ParentId;

		if(parentIndex != -1)
		{
			if(aIndex != aStopIndex)
			{
				SkeletonPoseComputeGlobal(apSkeleton, apSkeletonPoseLocal, apSkeletonPoseGlobal, parentIndex, aStopIndex);
			}

			apSkeletonPoseGlobal->m_X[aIndex] = xformMul( apSkeletonPoseGlobal->m_X[parentIndex], apSkeletonPoseLocal->m_X[aIndex]);
		}
		else
		{
			apSkeletonPoseGlobal->m_X[aIndex] = apSkeletonPoseLocal->m_X[aIndex];
		}
	}	

	void SkeletonPoseComputeGlobalQ(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseLocal, SkeletonPose* apSkeletonPoseGlobal)
	{
		apSkeletonPoseGlobal->m_X[0].q = apSkeletonPoseLocal->m_X[0].q;

		uint32_t i;
		for(i=1;i < apSkeleton->m_Count; i++)
		{
			apSkeletonPoseGlobal->m_X[i].q = normalize(quatMul( apSkeletonPoseGlobal->m_X[apSkeleton->m_Node[i].m_ParentId].q, apSkeletonPoseLocal->m_X[i].q));
		}
	}

	void SkeletonPoseComputeGlobalQ(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseLocal, SkeletonPose* apSkeletonPoseGlobal, int aIndex, int aStopIndex)
	{
		int parentIndex = apSkeleton->m_Node[aIndex].m_ParentId;

		if(parentIndex != -1)
		{
			if(aIndex != aStopIndex)
			{
				SkeletonPoseComputeGlobalQ(apSkeleton, apSkeletonPoseLocal, apSkeletonPoseGlobal, parentIndex, aStopIndex);
			}

			apSkeletonPoseGlobal->m_X[aIndex].q = normalize(quatMul( apSkeletonPoseGlobal->m_X[parentIndex].q, apSkeletonPoseLocal->m_X[aIndex].q));
		}
		else
		{
			apSkeletonPoseGlobal->m_X[aIndex].q = apSkeletonPoseLocal->m_X[aIndex].q;
		}
	}	

	void SkeletonPoseComputeLocal(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseGlobal, SkeletonPose* apSkeletonPoseLocal, int aIndex, int aStopIndex)
	{
		int parentIndex = apSkeleton->m_Node[aIndex].m_ParentId;

		if(parentIndex != -1)
		{
			apSkeletonPoseLocal->m_X[aIndex] = xformInvMul( apSkeletonPoseGlobal->m_X[parentIndex], apSkeletonPoseGlobal->m_X[aIndex]);

			if(aIndex != aStopIndex)
			{
				SkeletonPoseComputeLocal(apSkeleton, apSkeletonPoseGlobal, apSkeletonPoseLocal, parentIndex, aStopIndex);
			}
		}
		else
		{
			apSkeletonPoseLocal->m_X[aIndex] = apSkeletonPoseGlobal->m_X[aIndex];
		}
	}	

	void SkeletonPoseComputeLocalQ(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseGlobal, SkeletonPose* apSkeletonPoseLocal)
	{
		uint32_t i;
		for(i=apSkeleton->m_Count-1;i > 0; i--)
		{
			apSkeletonPoseLocal->m_X[i].q = normalize(quatMul( quatConj(apSkeletonPoseGlobal->m_X[apSkeleton->m_Node[i].m_ParentId].q), apSkeletonPoseGlobal->m_X[i].q));
		}

		apSkeletonPoseLocal->m_X[0].q = apSkeletonPoseGlobal->m_X[0].q;
	}

	void SkeletonPoseComputeLocalQ(Skeleton const* apSkeleton, SkeletonPose const* apSkeletonPoseGlobal, SkeletonPose* apSkeletonPoseLocal, int aIndex, int aStopIndex)
	{
		int parentIndex = apSkeleton->m_Node[aIndex].m_ParentId;

		if(parentIndex != -1)
		{
			apSkeletonPoseLocal->m_X[aIndex].q = normalize(quatMul( quatConj(apSkeletonPoseGlobal->m_X[parentIndex].q), apSkeletonPoseGlobal->m_X[aIndex].q));

			if(aIndex != aStopIndex)
			{
				SkeletonPoseComputeLocalQ(apSkeleton, apSkeletonPoseGlobal, apSkeletonPoseLocal, parentIndex, aStopIndex);
			}
		}
		else
		{
			apSkeletonPoseLocal->m_X[aIndex].q = apSkeletonPoseGlobal->m_X[aIndex].q;
		}
	}

	math::float4 SkeletonGetDoF(Skeleton const* apSkeleton,SkeletonPose const *apSkeletonPose, int32_t aIndex)
	{
		const int32_t axesIndex = apSkeleton->m_Node[aIndex].m_AxesId;
		return math::cond(math::bool4(axesIndex != -1),math::ToAxes(apSkeleton->m_AxesArray[axesIndex],apSkeletonPose->m_X[aIndex].q),math::quat2Qtan(apSkeletonPose->m_X[aIndex].q));
	}

	void SkeletonSetDoF(Skeleton const* apSkeleton,SkeletonPose * apSkeletonPose, math::float4 const& aDoF, int32_t aIndex)
	{
		const int32_t axesIndex = apSkeleton->m_Node[aIndex].m_AxesId;
		apSkeletonPose->m_X[aIndex].q = math::cond(math::bool4(axesIndex != -1),math::FromAxes(apSkeleton->m_AxesArray[axesIndex],aDoF),qtan2Quat(aDoF));
	}

	math::float4 SkeletonNodeEndPoint(Skeleton const *apSkeleton, int32_t aIndex, SkeletonPose const *apSkeletonPose)
	{
		return math::xformMulVec(apSkeletonPose->m_X[aIndex],math::quatXcos(apSkeleton->m_AxesArray[aIndex].m_PostQ) * math::float1(apSkeleton->m_AxesArray[aIndex].m_Length));
	}

	void SkeletonAlign(skeleton::Skeleton const *apSkeleton, math::float4 const &arRefQ, math::float4 & arQ, int32_t aIndex)
	{	
		const int32_t axesIndex = apSkeleton->m_Node[aIndex].m_AxesId;

		if(axesIndex != -1)
		{
			math::Axes axes = apSkeleton->m_AxesArray[axesIndex];

			math::float4 refV = math::quatXcos(math::normalize(math::quatMul(arRefQ,axes.m_PostQ)));
			math::float4 v = math::quatXcos(math::normalize(math::quatMul(arQ,axes.m_PostQ)));
			math::float4 dq = math::quatArcRotate(v,refV);
				
			arQ = math::normalize(math::quatMul(dq,arQ));
		}
	}

	void SkeletonAlign(skeleton::Skeleton const *apSkeleton, skeleton::SkeletonPose const*apSkeletonPoseRef, skeleton::SkeletonPose *apSkeletonPose, int32_t aIndex)
	{	
		SkeletonAlign(apSkeleton,apSkeletonPoseRef->m_X[aIndex].q,apSkeletonPose->m_X[aIndex].q,aIndex);
	}

	void Skeleton2BoneAdjustLength(Skeleton const *apSkeleton, int32_t aIndexA, int32_t aIndexB, int32_t aIndexC, math::float4 const &aTarget, math::float1 const& aRatio, SkeletonPose *apSkeletonPose, SkeletonPose *apSkeletonPoseWorkspace)
	{
		math::float4 vAB = apSkeletonPoseWorkspace->m_X[aIndexB].t - apSkeletonPoseWorkspace->m_X[aIndexA].t;
		math::float4 vBC = apSkeletonPoseWorkspace->m_X[aIndexC].t - apSkeletonPoseWorkspace->m_X[aIndexB].t;
		math::float4 vAD = aTarget - apSkeletonPoseWorkspace->m_X[aIndexA].t;

		math::float1 lenABC = math::length(vAB) + math::length(vBC);
		math::float1 lenAD = math::length(vAD);
		math::float1 ratio = lenAD / lenABC;
		math::float1 invARatio = math::float1::one() - aRatio;

		if(ratio > invARatio)
		{
			ratio = math::saturate( (ratio-(invARatio))/(math::float1(2)*aRatio) ); 
			math::float1 r = math::float1::one() + aRatio * ratio * ratio;
		
			apSkeletonPose->m_X[aIndexB].t *= r;
			apSkeletonPose->m_X[aIndexC].t *= r;
		}
	}

	void Skeleton2BoneIK(Skeleton const *apSkeleton, int32_t aIndexA, int32_t aIndexB, int32_t aIndexC, math::float4 const &aTarget, float aWeight, SkeletonPose *apSkeletonPose, SkeletonPose *apSkeletonPoseWorkspace)
	{
		math::float4 qA0 = apSkeletonPose->m_X[aIndexA].q;
		math::float4 qB0 = apSkeletonPose->m_X[aIndexB].q;

		math::float4 dof = skeleton::SkeletonGetDoF(apSkeleton,apSkeletonPose,aIndexB) * math::float4(1.0f,0,0.9f,0);
		skeleton::SkeletonSetDoF(apSkeleton,apSkeletonPose,dof,aIndexB);
		skeleton::SkeletonPoseComputeGlobal(apSkeleton,apSkeletonPose,apSkeletonPoseWorkspace,aIndexC,aIndexB);
	 
		math::float4 vAB = apSkeletonPoseWorkspace->m_X[aIndexB].t - apSkeletonPoseWorkspace->m_X[aIndexA].t;
		math::float4 vBC = apSkeletonPoseWorkspace->m_X[aIndexC].t - apSkeletonPoseWorkspace->m_X[aIndexB].t;
		math::float4 vAC = apSkeletonPoseWorkspace->m_X[aIndexC].t - apSkeletonPoseWorkspace->m_X[aIndexA].t;
		math::float4 vAD = aTarget - apSkeletonPoseWorkspace->m_X[aIndexA].t;

		math::float1 lenAB = math::length(vAB);
		math::float1 lenBC = math::length(vBC);
		math::float1 lenAC = math::length(vAC);
		math::float1 lenAD = math::length(vAD);

		math::float1 angleAC = math::triangleAngle(lenAC,lenAB,lenBC);
		math::float1 angleAD = math::triangleAngle(lenAD,lenAB,lenBC);

		math::float4 axis = math::normalize(math::cross(vAB,vBC));

		math::float1 a = math::float1(0.5f) * (angleAC-angleAD);
		math::float1 s,c;
		math::sincos(a,s,c);
		math::float4 q = axis*s;
		q.w() = c;
		apSkeletonPoseWorkspace->m_X[aIndexB].q = math::normalize(math::quatMul(q,apSkeletonPoseWorkspace->m_X[aIndexB].q));

		skeleton::SkeletonPoseComputeLocal(apSkeleton,apSkeletonPoseWorkspace,apSkeletonPose,aIndexB,aIndexB);
		apSkeletonPose->m_X[aIndexB].q = math::quatLerp(qB0,apSkeletonPose->m_X[aIndexB].q,math::float1(aWeight)); 
		skeleton::SkeletonPoseComputeGlobal(apSkeleton,apSkeletonPose,apSkeletonPoseWorkspace,aIndexC,aIndexB);

		vAC = apSkeletonPoseWorkspace->m_X[aIndexC].t - apSkeletonPoseWorkspace->m_X[aIndexA].t;
		
		q = math::normalize(math::quatArcRotate(vAC,vAD));
		apSkeletonPoseWorkspace->m_X[aIndexA].q = math::normalize(math::quatMul(q,apSkeletonPoseWorkspace->m_X[aIndexA].q));
		skeleton::SkeletonPoseComputeLocal(apSkeleton,apSkeletonPoseWorkspace,apSkeletonPose,aIndexA,aIndexA);
		apSkeletonPose->m_X[aIndexA].q = math::quatLerp(apSkeletonPose->m_X[aIndexA].q,qA0,math::float1(math::pow(1-aWeight,4))); 
	}

	void Skeleton3BoneIK(Skeleton const *apSkeleton, int32_t aIndexA, int32_t aIndexB, int32_t aIndexC, math::float4 const &aTarget, float weight, SkeletonPose *apSkeletonPose, SkeletonPose *apSkeletonPoseWorkspace)
	{
		if(weight > 0)
		{
			math::float4 qA = apSkeletonPose->m_X[aIndexA].q;
			math::float4 qB = apSkeletonPose->m_X[aIndexB].q;
			math::float4 qC = apSkeletonPose->m_X[aIndexC].q;

			float fingerLen = apSkeleton->m_AxesArray[aIndexA].m_Length + apSkeleton->m_AxesArray[aIndexB].m_Length + apSkeleton->m_AxesArray[aIndexC].m_Length; 
			math::float1 targetDist = math::length(apSkeletonPoseWorkspace->m_X[aIndexA].t - aTarget);
			float fact = math::pow(math::clamp(targetDist.tofloat()/fingerLen,0.f,1.f),4.0f);
			math::float4 dof(0,0,2.0f*fact-1.0f,0);

			skeleton::SkeletonSetDoF(apSkeleton,apSkeletonPose,dof,aIndexB);
			skeleton::SkeletonSetDoF(apSkeleton,apSkeletonPose,dof,aIndexC);
			skeleton::SkeletonPoseComputeGlobal(apSkeleton,apSkeletonPose,apSkeletonPoseWorkspace,aIndexC,aIndexB);

			math::float4 endT = SkeletonNodeEndPoint(apSkeleton,aIndexC,apSkeletonPoseWorkspace);
			math::float4 endV = endT - apSkeletonPoseWorkspace->m_X[aIndexA].t;
			math::float4 targetV = aTarget - apSkeletonPoseWorkspace->m_X[aIndexA].t;
			
			math::float4 q = math::normalize(math::quatArcRotate(endV,targetV));
			apSkeletonPoseWorkspace->m_X[aIndexA].q = math::quatMul(q,apSkeletonPoseWorkspace->m_X[aIndexA].q);
			skeleton::SkeletonPoseComputeLocal(apSkeleton,apSkeletonPoseWorkspace,apSkeletonPose,aIndexA,aIndexA);
		
			math::float1 w(weight);

			apSkeletonPose->m_X[aIndexA].q = math::quatLerp(qA,apSkeletonPose->m_X[aIndexA].q,w);
			apSkeletonPose->m_X[aIndexB].q = math::quatLerp(qB,apSkeletonPose->m_X[aIndexB].q,w);
			apSkeletonPose->m_X[aIndexC].q = math::quatLerp(qC,apSkeletonPose->m_X[aIndexC].q,w);
		}
	}

	void SetupAxes(skeleton::Skeleton *apSkeleton, skeleton::SkeletonPose const *apSkeletonPoseGlobal, SetupAxesInfo const& apSetupAxesInfo, int32_t aIndex, int32_t aAxisIndex, bool aLeft, float aLen)
	{
		skeleton::Node &node = apSkeleton->m_Node[aIndex];
		int32_t parentIndex = node.m_ParentId;
				
		if(node.m_AxesId != -1)
		{
			math::Axes &axes = apSkeleton->m_AxesArray[node.m_AxesId];

			math::xform boneX = apSkeletonPoseGlobal->m_X[aIndex];

			axes.m_Limit.m_Min = math::radians(math::float4(apSetupAxesInfo.m_Min[0],apSetupAxesInfo.m_Min[1],apSetupAxesInfo.m_Min[2],0));
			axes.m_Limit.m_Max = math::radians(math::float4(apSetupAxesInfo.m_Max[0],apSetupAxesInfo.m_Max[1],apSetupAxesInfo.m_Max[2],0));
			axes.m_Sgn = math::float4(apSetupAxesInfo.m_Sgn[0],apSetupAxesInfo.m_Sgn[1],apSetupAxesInfo.m_Sgn[2],1) * (aLeft ? math::float4(1) : math::float4(-1,1,-1,-1));

			math::float4 mainAxis = math::float4(apSetupAxesInfo.m_MainAxis[0],apSetupAxesInfo.m_MainAxis[1],apSetupAxesInfo.m_MainAxis[2],0);
			math::float4 zeroQ = math::float4(apSetupAxesInfo.m_PreQ[0],apSetupAxesInfo.m_PreQ[1],apSetupAxesInfo.m_PreQ[2],apSetupAxesInfo.m_PreQ[3]) * (aLeft ? math::float4(1) : math::float4(1,1,1,-1));

			math::float4 u(1,0,0,0);
			math::float4 w(0,1,0,0);
			math::float4 v(0,0,1,0);

			axes.m_Type = apSetupAxesInfo.m_Type; 
						
			axes.m_Length = 1.0f;

			if(aAxisIndex != -1)				
			{
				math::xform axisX = apSkeletonPoseGlobal->m_X[aAxisIndex];

				u = math::normalize((axisX.t - boneX.t) * math::float1(aLen));
	
				w = mainAxis;
				v = math::normalize(math::cross(w,u));
				w = cross(u,v);
			
				axes.m_Length = math::length(axisX.t - boneX.t).tofloat();
			}

			if(apSetupAxesInfo.m_ForceAxis)
			{
				switch(apSetupAxesInfo.m_ForceAxis)
				{
					case +1: u = math::float4(+1,0,0,0); break;
					case -1: u = math::float4(-1,0,0,0); break;
					case +2: u = math::float4(0,+1,0,0); break;
					case -2: u = math::float4(0,-1,0,0); break;
					case +3: u = math::float4(0,0,+1,0); break;
					default: u = math::float4(0,0,-1,0); break;
				};

				w = mainAxis;
				v = math::normalize(math::cross(w,u));
				w = cross(u,v);
			}

			axes.m_Length *= fabs(aLen);

			math::float4 parentQ = math::cond(math::bool4(parentIndex != -1),apSkeletonPoseGlobal->m_X[parentIndex].q, math::quatIdentity());

			axes.m_PreQ = math::quatMatrixToQuat(u,v,w);
			axes.m_PostQ = math::normalize(math::quatMul(math::quatConj(boneX.q),axes.m_PreQ));
			axes.m_PreQ = math::normalize(math::quatMul(math::quatConj(parentQ),math::quatMul(zeroQ,axes.m_PreQ)));
		}
	}

	static int GetSkeletonNodeDepth(mecanim::skeleton::Skeleton const& skeleton, uint32_t boneIndex)
	{	
		uint32_t parentIndex = skeleton.m_Node[boneIndex].m_ParentId;
		int depth = 0;
		while (parentIndex != -1)
		{
			depth++;
			parentIndex = skeleton.m_Node[parentIndex].m_ParentId;
		}
		return depth;	
	}
}
}

