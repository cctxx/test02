/*
 Copyright (c) 7244339 Canada Inc. (Mecanim)
 All Rights Reserved.
*/
#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/object.h"

#include "Runtime/Math/Simd/math.h" 

#include "Runtime/mecanim/graph/plug.h"
#include "Runtime/mecanim/graph/node.h"

#include "Runtime/mecanim/graph/unarynode.h"

#include "Runtime/mecanim/generic/stringtable.h"

namespace mecanim
{

namespace graph
{
	class IdentityOp
	{
	public:
		template<typename RESULT> static math::xform Operation(){ return math::xformIdentity(); } 
	};

	class xformIdentity : public ResultNode<math::xform, IdentityOp> 
	{
	public: 
		static const eNodeType mId = xformIdentityId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformMulVecOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::float4 Operation( math::xform const& l, math::float4 const& r){ return math::xformMulVec(l, r); } 
	};

	class xformMulVec : public BinaryNode<math::xform, math::float4, math::float4, xformMulVecOp> 
	{
	public: 
		static const eNodeType mId = xformMulVecId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformInvMulVecOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::float4 Operation( math::xform const& l, math::float4 const& r){ return math::xformInvMulVec(l, r); } 
	};

	class xformInvMulVec : public BinaryNode<math::xform, math::float4, math::float4, xformInvMulVecOp> 
	{
	public: 
		static const eNodeType mId = xformInvMulVecId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformMulOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::xform Operation( math::xform const& l, math::xform const& r){ return math::xformMul(l, r); } 
	};

	class xformMul : public BinaryNode<math::xform, math::xform, math::xform, xformMulOp> 
	{
	public: 
		static const eNodeType mId = xformMulId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformInvMulOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::xform Operation( math::xform const& l, math::xform const& r){ return math::xformInvMul(l, r); } 
	};

	class xformInvMul : public BinaryNode<math::xform, math::xform, math::xform, xformInvMulOp> 
	{
	public: 
		static const eNodeType mId = xformInvMulId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformMulInvOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::xform Operation( math::xform const& l, math::xform const& r){ return math::xformMulInv(l, r); } 
	};

	class xformMulInv : public BinaryNode<math::xform, math::xform, math::xform, xformMulInvOp> 
	{
	public: 
		static const eNodeType mId = xformMulInvId; 
		virtual eNodeType NodeType(){ return mId;}
	};

	class xformEqualOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static bool Operation( math::xform const& l, math::xform const& r){ return l == r; } 
	};

	class xformEqual : public BinaryNode<math::xform, math::xform, bool, xformEqualOp> 
	{
	public: 
		static const eNodeType mId = xformEqualId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformWeightOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::xform Operation( math::xform const& l, float r){ return math::xformWeight(l, math::float1(r)); } 
	};

	class xformWeight : public BinaryNode<math::xform, float, math::xform, xformWeightOp> 
	{
	public: 
		static const eNodeType mId = xformWeightId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformBlendOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename TYPE3, typename RESULT> static math::xform Operation( math::xform const& a,math::xform const& b, float const& c){ return math::xformBlend(a,b,math::float1(c)); } 
	};

	class xformBlend : public TernaryNode<math::xform, math::xform, float, math::xform, xformBlendOp> 
	{
	public: 
		static const eNodeType mId = xformBlendId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformAddOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::xform Operation( math::xform const& l, math::xform const& r){ return math::xformAdd(l, r); } 
	};

	class xformAdd : public BinaryNode<math::xform, math::xform, math::xform, xformAddOp> 
	{
	public: 
		static const eNodeType mId = xformAddId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformSubOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::xform Operation( math::xform const& l, math::xform const& r){ return math::xformSub(l, r); } 
	};

	class xformSub : public BinaryNode<math::xform, math::xform, math::xform, xformSubOp> 
	{
	public: 
		static const eNodeType mId = xformSubId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformComposeOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename TYPE3, typename RESULT> static math::xform Operation( math::float4 const& t, math::float4 const& q, math::float4 const& s){ return math::xform(t, q, s); } 
	};

	class xformCompose : public TernaryNode<math::float4, math::float4, math::float4, math::xform, xformComposeOp> 
	{
	public: 
		xformCompose()
		{
			mA.m_ID = CRCKey(eT);
			mB.m_ID = CRCKey(eQ);
			mC.m_ID = CRCKey(eS);
		}
		static const eNodeType mId = xformComposeId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class xformDecompose : public Node
	{
	public:
		static const eNodeType mId = xformDecomposeId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<math::xform> mX;
		TypePlug<math::float4> mT;
		TypePlug<math::float4> mQ;
		TypePlug<math::float4> mS;

		xformDecompose()
			:mX(true,CRCKey(eX)),
			mT(false,CRCKey(eT)),
			mQ(false,CRCKey(eQ)),
			mS(false,CRCKey(eS))
		{
			mX.m_Owner = this;
			mT.m_Owner = this;
			mQ.m_Owner = this;
			mS.m_Owner = this;
		}

		virtual ~xformDecompose(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 4;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mX;
				case 1: return mT;
				case 2: return mQ;
				case 3: 
				default:return mS;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mX;
				case 1: return mT;
				case 2: return mQ;
				case 3: 
				default:return mS;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			math::xform x;
			
			mX.ReadData(&x, arEvaluationInfo);	
			
			mT.WriteData(&x.t, arEvaluationInfo);
			mQ.WriteData(&x.q, arEvaluationInfo);
			mS.WriteData(&x.s, arEvaluationInfo);
		}
	};

	class xformRefChange : public Node
	{
	public:
		static const eNodeType mId = xformRefChangeId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<math::xform> mSrcRefX;
		TypePlug<math::xform> mDstRefX;
		TypePlug<math::xform> mSrcPivotX;
		TypePlug<math::xform> mDstPivotX;
		TypePlug<float> mRefWeight;
		TypePlug<float> mPivotWeight;
		TypePlug<math::xform> mXI;
		TypePlug<math::xform> mXO;

		xformRefChange()	:	mSrcRefX(true,CRCKey(eSrcRefX)),
								mDstRefX(true,CRCKey(eDstRefX)),
								mSrcPivotX(true,CRCKey(eSrcPivotX)),
								mDstPivotX(true,CRCKey(eDstPivotX)),
								mRefWeight(true,CRCKey(eRefWeight)),
								mPivotWeight(true,CRCKey(ePivotWeight)),
								mXI(true,CRCKey(eXI)),
								mXO(false,CRCKey(eXO))
		{
			mSrcRefX.m_Owner = this;
			mDstRefX.m_Owner = this;
			mSrcPivotX.m_Owner = this;
			mDstPivotX.m_Owner = this;
			mRefWeight.m_Owner = this;
			mPivotWeight.m_Owner = this;
			mXI.m_Owner = this;
			mXO.m_Owner = this;
		}

		virtual ~xformRefChange(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 8;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0:		return mSrcRefX;
				case 1:		return mDstRefX;
				case 2:		return mSrcPivotX;
				case 3:		return mDstPivotX;
				case 4:		return mRefWeight;
				case 5:		return mPivotWeight;
				case 6:		return mXI;
				case 7:
				default:	return mXO;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0:		return mSrcRefX;
				case 1:		return mDstRefX;
				case 2:		return mSrcPivotX;
				case 3:		return mDstPivotX;
				case 4:		return mRefWeight;
				case 5:		return mPivotWeight;
				case 6:		return mXI;
				case 7:
				default:	return mXO;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			math::xform srcRefX, dstRefX, srcPivotX, dstPivotX, xi, xo;
			float rw,pw;
			
			mSrcRefX.ReadData(&srcRefX, arEvaluationInfo);	
			mDstRefX.ReadData(&dstRefX, arEvaluationInfo);	
			mSrcPivotX.ReadData(&srcPivotX, arEvaluationInfo);	
			mDstPivotX.ReadData(&dstPivotX, arEvaluationInfo);	
			mRefWeight.ReadData(&rw, arEvaluationInfo);	
			mPivotWeight.ReadData(&pw, arEvaluationInfo);	
			mXI.ReadData(&xi,arEvaluationInfo);	

			xo = math::xformMul(xi,srcPivotX);
			xo = math::xformInvMul(srcRefX,xo);
			xo = math::xformWeight(xo,math::float1(1-pw));
			xo = math::xformMul(dstRefX,xo);
			xo = math::xformMulInv(xo,dstPivotX);
			xo = math::xformBlend(xi,xo,math::float1(rw));

			mXO.WriteData(&xo, arEvaluationInfo);
		}
	};
}

}