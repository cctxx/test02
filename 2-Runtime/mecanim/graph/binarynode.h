/*
 Copyright (c) 7244339 Canada Inc. (Mecanim)
 All Rights Reserved.
*/
#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/object.h"

#include "Runtime/Math/Simd/float4.h" 

#include "Runtime/mecanim/graph/plug.h"
#include "Runtime/mecanim/graph/node.h"

namespace mecanim
{

namespace graph
{
	template <typename TYPE1, typename TYPE2, typename RESULT, typename BinaryPolicies> class BinaryNode : public Node
	{
	public:
		TypePlug<TYPE1> mA;
		TypePlug<TYPE2> mB;

		TypePlug<RESULT> mResult;

		BinaryNode()
			:mA(true, CRCKey(eA) ),
			mB(true,CRCKey(eB)),
			mResult(false,CRCKey(eResult))
		{
			mA.m_Owner = this;
			mB.m_Owner = this;
			mResult.m_Owner = this;
		}

		virtual ~BinaryNode(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 3;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mA;
				case 1: return mB;
				case 2:
				default: return mResult;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mA;
				case 1: return mB;
				case 2:
				default: return mResult;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			TYPE1 a;
			TYPE2 b;
			RESULT result;

			mA.ReadData(&a, arEvaluationInfo);
			mB.ReadData(&b, arEvaluationInfo);

			result = BinaryPolicies::template Operation<TYPE1, TYPE2, RESULT>(a, b);

			mResult.WriteData(&result, arEvaluationInfo);
		}
	};

	class AdditionOp 
	{
	public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return l+r; } 
	};

	class SubstractionOp 
	{
	public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return l-r; } 
	};

	class MultiplicationOp 
	{
	public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return l*r; } 
	};

	class DivisionOp 
	{
	public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return l/r; } 
	};

	class GreaterThanOp
	{
		public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return l > r; } 
	};
	class LesserThanOp
	{
		public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return l < r; } 
	};
	class GreaterThanOrEqualOp
	{
		public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return l >= r; } 
	};
	class LesserThanOrEqualOp
	{
		public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return l <= r; } 
	};
	class AndOp
	{
		public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return l && r; } 
	};
	class OrOp
	{
		public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return l || r; } 
	};
	class XorOp
	{
		public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l,  TYPE2 const& r){ return ((!l)&&r)||(l&&(!r)); } 
	};

	class AdditionFloat  : public BinaryNode<float, float, float, AdditionOp> 
	{ 
	public: 
		static const eNodeType mId = AdditionFloatId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class AdditionUInt   : public BinaryNode<uint32_t, uint32_t, uint32_t,AdditionOp> 
	{
	public:
		static const eNodeType mId = AdditionUIntId;
		virtual eNodeType NodeType(){return mId;}
	};
	class AdditionInt    : public BinaryNode<int32_t, int32_t, int32_t, AdditionOp> 
	{
	public:
		static const eNodeType mId = AdditionIntId;
		virtual eNodeType NodeType(){return mId;}
	};
	class AdditionFloat4 : public BinaryNode<math::float4, math::float4, math::float4, AdditionOp> 
	{
	public:
		static const eNodeType mId = AdditionFloat4Id;
		virtual eNodeType NodeType(){return mId;}
	};

	class SubstractionFloat  : public BinaryNode<float, float, float, SubstractionOp> 
	{
	public:
		static const eNodeType mId = SubstractionFloatId;
		virtual eNodeType NodeType(){return mId;}
	};
	class SubstractionUInt   : public BinaryNode<uint32_t, uint32_t, uint32_t, SubstractionOp> 
	{
	public:
		static const eNodeType mId = SubstractionUIntId;
		virtual eNodeType NodeType(){return mId;}
	};
	class SubstractionInt    : public BinaryNode<int32_t, int32_t, int32_t, SubstractionOp> 
	{
	public:
		static const eNodeType mId = SubstractionIntId;
		virtual eNodeType NodeType(){return mId;}
	};
	class SubstractionFloat4 : public BinaryNode<math::float4, math::float4, math::float4, SubstractionOp> 
	{
	public:
		static const eNodeType mId = SubstractionFloat4Id;
		virtual eNodeType NodeType(){return mId;}
	};

	class MultiplicationFloat  : public BinaryNode<float, float, float, MultiplicationOp> 
	{
	public:
		static const eNodeType mId = MultiplicationFloatId;
		virtual eNodeType NodeType(){return mId;}
	};
	class MultiplicationUInt   : public BinaryNode<uint32_t, uint32_t, uint32_t, MultiplicationOp> 
	{
	public:
		static const eNodeType mId = MultiplicationUIntId;
		virtual eNodeType NodeType(){return mId;}
	};
	class MultiplicationInt    : public BinaryNode<int32_t, int32_t, int32_t, MultiplicationOp> 
	{
	public:
		static const eNodeType mId = MultiplicationIntId;
		virtual eNodeType NodeType(){return mId;}
	};
	class MultiplicationFloat4 : public BinaryNode<math::float4, math::float4, math::float4, MultiplicationOp> 
	{
	public:
		static const eNodeType mId = MultiplicationFloat4Id;
		virtual eNodeType NodeType(){return mId;}
	};

	class DivisionFloat  : public BinaryNode<float, float, float, DivisionOp> 
	{
	public:
		static const eNodeType mId = DivisionFloatId;
		virtual eNodeType NodeType(){return mId;}
	};
	class DivisionUInt   : public BinaryNode<uint32_t, uint32_t, uint32_t, DivisionOp> 
	{
	public:
		static const eNodeType mId = DivisionUIntId;
		virtual eNodeType NodeType(){return mId;}
	};
	class DivisionInt    : public BinaryNode<int32_t, int32_t, int32_t, DivisionOp> 
	{
	public:
		static const eNodeType mId = DivisionIntId;
		virtual eNodeType NodeType(){return mId;}
	};
	class DivisionFloat4 : public BinaryNode<math::float4, math::float4, math::float4, DivisionOp> 
	{
	public:
		static const eNodeType mId = DivisionFloat4Id;
		virtual eNodeType NodeType(){return mId;}
	};

	class GreaterThan : public BinaryNode<float, float, bool, GreaterThanOp> 
	{
	public:
		static const eNodeType mId = GreaterThanId;
		virtual eNodeType NodeType(){return mId;}
	};

	class LesserThan : public BinaryNode<float, float, bool, LesserThanOp> 
	{
	public:
		static const eNodeType mId = LesserThanId;
		virtual eNodeType NodeType(){return mId;}
	};

	class GreaterThanOrEqual : public BinaryNode<float, float, bool, GreaterThanOrEqualOp> 
	{
	public:
		static const eNodeType mId = GreaterThanOrEqualId;
		virtual eNodeType NodeType(){return mId;}
	};

	class LesserThanOrEqual : public BinaryNode<float, float, bool, LesserThanOrEqualOp> 
	{
	public:
		static const eNodeType mId = LesserThanOrEqualId;
		virtual eNodeType NodeType(){return mId;}
	};

	class And : public BinaryNode<bool, bool, bool, AndOp> 
	{
	public:
		static const eNodeType mId = AndId;
		virtual eNodeType NodeType(){return mId;}
	};

	class Or : public BinaryNode<bool, bool, bool, OrOp> 
	{
	public:
		static const eNodeType mId = OrId;
		virtual eNodeType NodeType(){return mId;}
	};
	class Xor : public BinaryNode<bool, bool, bool, XorOp> 
	{
	public:
		static const eNodeType mId = XorId;
		virtual eNodeType NodeType(){return mId;}
	};
}

}