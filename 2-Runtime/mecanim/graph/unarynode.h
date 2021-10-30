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

namespace mecanim
{

namespace graph
{
	template <typename TYPE, typename RESULT, typename UnaryPolicies> class UnaryNode : public Node
	{
	public:
		TypePlug<TYPE> mA;

		TypePlug<RESULT> mResult;

		UnaryNode()
			:mA(true,CRCKey(eA)),
			mResult(false,CRCKey(eResult))
		{		
			mA.m_Owner = this;
			mResult.m_Owner = this;
		}
		virtual ~UnaryNode(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 2;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mA;
				case 1:
				default: return mResult;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mA;
				case 1:
				default: return mResult;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			TYPE a;
			RESULT result;

			mA.ReadData(&a, arEvaluationInfo);

			result = UnaryPolicies::Operation(a);

			mResult.WriteData(&result, arEvaluationInfo);
		}
	};

	
	template< typename TYPE, typename RESULT > class NegationOp 
	{
	public:
		static RESULT Operation( TYPE const& l){ return -l; }		
	};

	template<> class NegationOp<bool, bool>
	{
	public:
		static bool Operation(bool const &l) { return !l; }	
	};

	class NegationFloat : public UnaryNode<float, float, NegationOp<float, float> > 
	{ 
	public: 
		static const eNodeType mId = NegationFloatId;
		virtual eNodeType NodeType(){return mId;}
	};
	class NegationInt : public UnaryNode<int32_t, int32_t, NegationOp<int32_t, int32_t> > 
	{ 
	public: 
		static const eNodeType mId = NegationIntId;
		virtual eNodeType NodeType(){return mId;}
	};
	class NegationFloat4 : public UnaryNode<math::float4, math::float4, NegationOp<math::float4, math::float4> > 
	{ 
	public:
		static const eNodeType mId = NegationFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};
	class NegationBool : public UnaryNode<bool, bool,  NegationOp<bool, bool> > 
	{ 
	public:
		static const eNodeType mId = NegationBoolId; 
		virtual eNodeType NodeType(){return mId;}
	};
}

}
