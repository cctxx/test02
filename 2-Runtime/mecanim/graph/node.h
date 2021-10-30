/*
 Copyright (c) 7244339 Canada Inc. (Mecanim)
 All Rights Reserved.
*/
#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/object.h"
#include "Runtime/mecanim/generic/stringtable.h"

#include "Runtime/mecanim/graph/plug.h"

namespace mecanim
{

namespace graph
{
	// If you add a new node id, add it at the end of this list, otherwise you will break
	// saved graph asset.
	enum eNodeType {
		InvalidNodeId,
		NegationFloatId,
		NegationIntId,
		NegationFloat4Id,
		AdditionFloatId,
		AdditionUIntId,
		AdditionIntId,
		AdditionFloat4Id,
		SubstractionFloatId,
		SubstractionUIntId,
		SubstractionIntId,
		SubstractionFloat4Id,
		MultiplicationFloatId,
		MultiplicationUIntId,
		MultiplicationIntId,
		MultiplicationFloat4Id,
		DivisionFloatId,
		DivisionUIntId,
		DivisionIntId,
		DivisionFloat4Id,
		CondFloatId,
		CondUIntId,
		CondIntId,
		CondFloat4Id,
		AbsFloatId, 
		AbsFloat4Id,
		CrossFloat4Id,
		DegreesFloatId,
		DegreesFloat4Id,
		DotFloat4Id,
		LengthFloat4Id,
		MaximumFloatId,
		MaximumUIntId,
		MaximumIntId, 
		MaximumFloat4Id,
		MinimumFloatId, 
		MinimumUIntId,
		MinimumIntId,
		MinimumFloat4Id,
		NormalizeFloat4Id,
		RadiansFloatId,
		RadiansFloat4Id,
		FloatToFloat4Id,
		Float4ToFloatId,
		GreaterThanId,
		LesserThanId,
		SmoothstepFloatId,
		Mux5FloatId,
		Mul5FloatId,
		FmodId,
		SinId,
		AndId,
		OrId,
		NegationBoolId,
		GreaterThanOrEqualId,
		LesserThanOrEqualId,
		xformMulInvId,
		xformIdentityId,
		xformMulVecId,
		xformInvMulVecId,
		xformMulId,
		xformInvMulId,
		xformEqualId,
		xformWeightId,
		xformAddId,
		xformSubId,
		quatIdentityId,
		quatConjId,
		quatMulId,
		quatMulVecId,
		quatLerpId,
		quatArcRotateId,
		quatArcRotateXId,
		quatXcosId,
		quatYcosId,
		quatZcosId,
		quatEulerToQuatId,
		quatQuatToEulerId,
		quatProjOnYPlaneId,
		quat2QtanId,
		qtan2QuatId,
		ZYRoll2QuatId,
		quat2ZYRollId,
		RollZY2QuatId,
		quat2RollZYId,
		quatWeightId,
		xformComposeId,
		xformDecomposeId,
		CondXformId,
		xformBlendId,
		FloatToFloat1Id, // Not used anymore
		Float1ToFloatId, // Not used anymore
		RandId,
		DampId,
		xformRefChangeId,
		XorId,
		SmoothPulseFloatId,
		InputId,
		OutputId,
		LastNodeId
	};

	class Node : public Object
	{
	public:		
		uint32_t	mID;

		Node(){}
		virtual ~Node(){}

		virtual eNodeType NodeType()=0;
		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)=0;
	
		virtual uint32_t GetPlugCount()const=0;
		virtual GraphPlug& GetPlug(uint32_t aIndex)=0;
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const=0;
	};

	template<typename TYPE> class TypePlug : public GraphPlug
	{
	public:
		typedef TYPE		value_type;
		typedef TYPE&		reference;
		typedef TYPE const&	const_reference;
		typedef TYPE*		pointer; 
		typedef TYPE const* const_pointer;	
		
		TypePlug(){}
		TypePlug(bool input, uint32_t id, Node* apOwner=0):GraphPlug(input, id, apOwner){}

		virtual std::size_t ValueSize()const
		{
			return sizeof(value_type);
		}

		virtual std::size_t ValueAlign()const
		{
			return ALIGN_OF(value_type);
		}

		virtual ePlugType	PlugType()const
		{
			return Trait<value_type>::PlugType();
		}

		void ConnectTo(TypePlug& arPlug)
		{
			m_Source = &arPlug;
		}

		bool ReadData(void* apData, EvaluationInfo& arEvaluationInfo)const
		{
			uint32_t offset = m_Offset;
			if(m_Source)
			{
				offset = m_Source->m_Offset;
				if(arEvaluationInfo.m_DataBlock.IsDirty(m_Source->m_PlugId, arEvaluationInfo.m_EvaluationId))
				{
					m_Source->m_Owner->Evaluate(arEvaluationInfo);	
				}				
			}
			
			*reinterpret_cast<pointer>(apData) = arEvaluationInfo.m_DataBlock.GetData<value_type>(offset);						
						
			return true;
		}

		bool WriteData(void const* apData, EvaluationInfo& arEvaluationInfo)const
		{
			arEvaluationInfo.m_DataBlock.SetData(*reinterpret_cast<const_pointer>(apData), m_PlugId, m_Offset, arEvaluationInfo.m_EvaluationId);
			return true;
		}
	};


	class GraphOutput : public Node
	{
	public:
		uint32_t	mPlugCount;
		GraphPlug**	mPlugArray;

		GraphOutput():mPlugCount(0), mPlugArray(0){}
		virtual ~GraphOutput(){}

		virtual eNodeType NodeType(){return OutputId;}

		virtual uint32_t GetPlugCount()const{return mPlugCount;}
		virtual GraphPlug& GetPlug(uint32_t aIndex){return *(mPlugArray[aIndex]);}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const{return *(mPlugArray[aIndex]);}


		virtual void Evaluate(EvaluationInfo& arEvaluationInfo){};
	};

	class GraphInput : public Node
	{
	public:
		uint32_t	mPlugCount;
		GraphPlug**	mPlugArray;

		GraphInput():mPlugCount(0), mPlugArray(0){}
		virtual ~GraphInput(){}

		virtual eNodeType NodeType(){return InputId;}

		virtual uint32_t GetPlugCount()const{return mPlugCount;}
		virtual GraphPlug& GetPlug(uint32_t aIndex){return *(mPlugArray[aIndex]);}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const{return *(mPlugArray[aIndex]);}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo){};
	};

	template <typename RESULT, typename ResultPolicies> class ResultNode : public Node
	{
	public:
		TypePlug<RESULT> mResult;

		ResultNode()
			:mResult(false,CRCKey(eResult))
		{		
			mResult.m_Owner = this;
		}
		virtual ~ResultNode(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 1;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0:
				default: return mResult;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: 
				default: return mResult;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			RESULT result = ResultPolicies::template Operation<RESULT>();
			mResult.WriteData(&result, arEvaluationInfo);
		}
	};

	template <typename TYPE1, typename TYPE2, typename TYPE3, typename RESULT, typename TernaryPolicies> class TernaryNode : public Node
	{
	public:
		TypePlug<TYPE1> mA;
		TypePlug<TYPE2> mB;
		TypePlug<TYPE3> mC;

		TypePlug<RESULT> mResult;

		TernaryNode()
			:mA(true, CRCKey(eA)),
			mB(true,CRCKey(eB)),
			mC(true,CRCKey(eC)),
			mResult(false,CRCKey(eResult))
		{
			mA.m_Owner = this;
			mB.m_Owner = this;
			mC.m_Owner = this;
			mResult.m_Owner = this;
		}

		virtual ~TernaryNode(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 4;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mA;
				case 1: return mB;
				case 2: return mC;
				case 3:
				default: return mResult;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mA;
				case 1: return mB;
				case 2: return mC;
				case 3:
				default: return mResult;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			TYPE1 a;
			TYPE2 b;
			TYPE3 c;
			RESULT result;

			mA.ReadData(&a, arEvaluationInfo);
			mB.ReadData(&b, arEvaluationInfo);
			mC.ReadData(&c, arEvaluationInfo);

			result = TernaryPolicies::template Operation<TYPE1, TYPE2, TYPE3, RESULT>(a, b, c);

			mResult.WriteData(&result, arEvaluationInfo);
		}
	};



}

}
