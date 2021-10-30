/*
 Copyright (c) 7244339 Canada Inc. (Mecanim)
 All Rights Reserved.
*/
#pragma once

#include <cstdlib>

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
	template <typename COND, typename TYPE, typename POLICIES> class CondNode : public Node
	{
	public:
		TypePlug<COND> mCondition;
		TypePlug<TYPE> mA;
		TypePlug<TYPE> mB;

		TypePlug<TYPE> mResult;

		CondNode()
			:mCondition(true,CRCKey(eCondition)),
			mA(true, CRCKey(eA)),
			mB(true,CRCKey(eB)),
			mResult(false,CRCKey(eResult))
		{
			mCondition.m_Owner = this;
			mA.m_Owner = this;
			mB.m_Owner = this;
			mResult.m_Owner = this;
		}

		virtual ~CondNode(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 4;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mCondition;
				case 1: return mA;
				case 2: return mB;
				case 3: 
				default: return mResult;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mCondition;
				case 1: return mA;
				case 2: return mB;
				case 3: 
				default: return mResult;
			}
		}


		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			TYPE a, b, result;
			COND cond;

			mCondition.ReadData(&cond, arEvaluationInfo);
			mA.ReadData(&a, arEvaluationInfo);
			mB.ReadData(&b, arEvaluationInfo);

			result = POLICIES::template Operation<COND, TYPE>(cond, a, b);

			mResult.WriteData(&result, arEvaluationInfo);
		}
	};

	class CondFloatOp 
	{
	public:
		template< typename COND, typename TYPE > static TYPE Operation( COND const& p, TYPE const& l, TYPE const& r){ return math::cond(p, l, r); } 
	};
	class CondUIntOp 
	{
	public:
		template< typename COND, typename TYPE > static TYPE Operation( COND const& p, TYPE const& l, TYPE const& r){ return math::cond(p, l, r); } 
	};
	class CondIntOp 
	{
	public:
		template< typename COND, typename TYPE > static TYPE Operation( COND const& p, TYPE const& l, TYPE const& r){ return math::cond(p, l, r); } 
	};
	class CondFloat4Op
	{
	public:
		template< typename COND, typename TYPE > static math::float4 Operation( bool const& p, math::float4 const& l, math::float4 const& r){ return math::cond(math::bool1(p), l, r); } 
	};
	class CondXformOp 
	{
	public:
		template< typename COND, typename TYPE > static TYPE Operation( COND const& p, TYPE const& l, TYPE const& r){ return math::cond(p, l, r); } 
	};

	class CondFloat : public CondNode<bool, float, CondFloatOp> 
	{
	public: 
		static const eNodeType mId = CondFloatId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class CondUInt : public CondNode<bool, uint32_t, CondUIntOp> 
	{
	public: 
		static const eNodeType mId = CondUIntId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class CondInt : public CondNode<bool, int32_t, CondIntOp> 
	{
	public: 
		static const eNodeType mId = CondIntId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class CondFloat4 : public CondNode<bool, math::float4, CondFloat4Op> 
	{
	public: 
		static const eNodeType mId = CondFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};
	class CondXform : public CondNode<bool, math::xform, CondXformOp> 
	{
	public: 
		static const eNodeType mId = CondXformId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template< typename TYPE, typename RESULT > class AbsOp 
	{
	public:
		static RESULT Operation( TYPE const& l){ return math::abs(l); } 
	};

	class AbsFloat : public UnaryNode<float, float, AbsOp<float, float> > 
	{
	public: 
		static const eNodeType mId = AbsFloatId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class AbsFloat4 : public UnaryNode<math::float4, math::float4, AbsOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = AbsFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};

	class CrossOp 
	{
	public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l, TYPE2 const& r){ return math::cross(l, r); } 
	};

	class CrossFloat4 : public BinaryNode<math::float4, math::float4, math::float4, CrossOp> 
	{
	public: 
		static const eNodeType mId = CrossFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};
	
	template< typename TYPE, typename RESULT > class DegreesOp 
	{
	public:
		 static RESULT Operation( TYPE const& l){ return math::degrees(l); } 
	};

	class DegreesFloat : public UnaryNode<float, float, DegreesOp<float, float> > 
	{
	public: 
		static const eNodeType mId = DegreesFloatId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class DegreesFloat4 : public UnaryNode<math::float4, math::float4, DegreesOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = DegreesFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};

	class DotOp 
	{
	public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l, TYPE2 const& r){ return float(math::dot(l, r)); } 
	};

	class DotFloat4 : public BinaryNode<math::float4, math::float4, float, DotOp> 
	{
	public: 
		static const eNodeType mId = DotFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};

	template< typename TYPE, typename RESULT > class LengthOp 
	{
	public:
		static RESULT Operation( TYPE const& l){ return float(math::length(l)); } 
	};

	class LengthFloat4 : public UnaryNode<math::float4, float, LengthOp<math::float4, float> > 
	{
	public: 
		static const eNodeType mId = LengthFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};

	class MaximumOp 
	{
	public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l, TYPE2 const& r){ return math::maximum(l, r); } 
	};

	class MaximumFloat : public BinaryNode<float, float, float, MaximumOp> 
	{
	public: 
		static const eNodeType mId = MaximumFloatId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class MaximumUInt : public BinaryNode<uint32_t, uint32_t, uint32_t, MaximumOp> 
	{
	public: 
		static const eNodeType mId = MaximumUIntId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class MaximumInt : public BinaryNode<int32_t, int32_t, int32_t, MaximumOp> 
	{
	public: 
		static const eNodeType mId = MaximumIntId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class MaximumFloat4 : public BinaryNode<math::float4, math::float4, math::float4, MaximumOp> 
	{
	public: 
		static const eNodeType mId = MaximumFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};

	class MinimumOp 
	{
	public:
		template< typename TYPE1, typename TYPE2, typename RESULT > static RESULT Operation( TYPE1 const& l, TYPE2 const& r){ return math::minimum(l, r); } 
	};

	class MinimumFloat : public BinaryNode<float, float, float, MinimumOp> 
	{
	public: 
		static const eNodeType mId = MinimumFloatId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class MinimumUInt : public BinaryNode<uint32_t, uint32_t, uint32_t, MinimumOp> 
	{
	public: 
		static const eNodeType mId = MinimumUIntId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class MinimumInt : public BinaryNode<int32_t, int32_t, int32_t, MinimumOp> 
	{
	public: 
		static const eNodeType mId = MinimumIntId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class MinimumFloat4 : public BinaryNode<math::float4, math::float4, math::float4, MinimumOp> 
	{
	public: 
		static const eNodeType mId = MinimumFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};	

	template< typename TYPE, typename RESULT > class NormalizeOp 
	{
	public:
		static RESULT Operation( TYPE const& l){ return math::normalize(l); } 
	};

	class NormalizeFloat4 : public UnaryNode<math::float4, math::float4, NormalizeOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = NormalizeFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};

	template< typename TYPE, typename RESULT > class RadiansOp 
	{
	public:
		static RESULT Operation( TYPE const& l){ return math::radians(l); } 
	};

	class RadiansFloat : public UnaryNode<float, float, RadiansOp<float, float> > 
	{
	public: 
		static const eNodeType mId = RadiansFloatId; 
		virtual eNodeType NodeType(){return mId;}
	};
	class RadiansFloat4 : public UnaryNode<math::float4, math::float4, RadiansOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = RadiansFloat4Id; 
		virtual eNodeType NodeType(){return mId;}
	};	
	
	class Float4ToFloat : public Node
	{
	public:
		static const eNodeType mId = Float4ToFloatId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<math::float4> mA;
		TypePlug<float> mX;
		TypePlug<float> mY;
		TypePlug<float> mZ;
		TypePlug<float> mW;

		Float4ToFloat()
			:mA(true, CRCKey(eA)),
			mX(false, CRCKey(eX)),
			mY(false, CRCKey(eY)),
			mZ(false, CRCKey(eZ)),
			mW(false, CRCKey(eW))
		{
			mA.m_Owner = this;
			mX.m_Owner = this;
			mY.m_Owner = this;
			mZ.m_Owner = this;
			mW.m_Owner = this;
		}

		virtual ~Float4ToFloat(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 5;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mA;
				case 1: return mX;
				case 2: return mY;
				case 3: return mZ;
				case 4:
				default: return mW;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mA;
				case 1: return mX;
				case 2: return mY;
				case 3: return mZ;
				case 4:
				default: return mW;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			math::float4 a;
			
			mA.ReadData(&a, arEvaluationInfo);	

			float ATTRIBUTE_ALIGN(ALIGN4F) b[4];

			math::store(a, b);
			
			mX.WriteData(&b[0], arEvaluationInfo);
			mY.WriteData(&b[1], arEvaluationInfo);
			mZ.WriteData(&b[2], arEvaluationInfo);
			mW.WriteData(&b[3], arEvaluationInfo);
		}
	};

	class FloatToFloat4 : public Node
	{
	public:
		static const eNodeType mId = FloatToFloat4Id;
		virtual eNodeType NodeType(){return mId;}

		TypePlug<float> mX;
		TypePlug<float> mY;
		TypePlug<float> mZ;
		TypePlug<float> mW;

		TypePlug<math::float4> mResult;
		

		FloatToFloat4()
			:mX(true,CRCKey(eX)),
			mY(true,CRCKey(eY)),
			mZ(true,CRCKey(eZ)),
			mW(true,CRCKey(eW)),
			mResult(false,CRCKey(eResult))
		{			
			mX.m_Owner = this;
			mY.m_Owner = this;
			mZ.m_Owner = this;
			mW.m_Owner = this;
			mResult.m_Owner = this;
		}

		virtual ~FloatToFloat4(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 5;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mResult;
				case 1: return mX;
				case 2: return mY;
				case 3: return mZ;
				case 4:
				default: return mW;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mResult;
				case 1: return mX;
				case 2: return mY;
				case 3: return mZ;
				case 4:
				default: return mW;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			float ATTRIBUTE_ALIGN(ALIGN4F) a[4];
			
			mX.ReadData(&a[0], arEvaluationInfo);
			mY.ReadData(&a[1], arEvaluationInfo);
			mZ.ReadData(&a[2], arEvaluationInfo);
			mW.ReadData(&a[3], arEvaluationInfo);

			math::float4 b = math::load(a);
			mResult.WriteData(&b, arEvaluationInfo);		
		}
	};


	class SmoothStepFloat : public Node
	{
	public:
		static const eNodeType mId = SmoothstepFloatId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<float> mMin;
		TypePlug<float> mMax;
		TypePlug<float> mValue;
		TypePlug<float> mResult;

		SmoothStepFloat()
			:mMin(true, CRCKey(eMin)),
			mMax(true,CRCKey(eMax)),
			mValue(true,CRCKey(eValue)),
			mResult(false,CRCKey(eResult))
		{
			mMin.m_Owner = this;
			mMax.m_Owner = this;
			mValue.m_Owner = this;
			mResult.m_Owner = this;
		}

		virtual ~SmoothStepFloat(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 4;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mMin;
				case 1: return mMax;
				case 2: return mValue;
				case 3: 
				default: return mResult;
				
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mMin;
				case 1: return mMax;
				case 2: return mValue;
				case 3: 
				default: return mResult;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			float min, max, value;
			
			mMin.ReadData(&min, arEvaluationInfo);	
			mMax.ReadData(&max, arEvaluationInfo);	
			mValue.ReadData(&value, arEvaluationInfo);	

			float ret = math::smoothstep( min, max, value);
			
			mResult.WriteData(&ret, arEvaluationInfo);
		}
	};

	class SmoothPulseFloat : public Node
	{
	public:
		static const eNodeType mId = SmoothPulseFloatId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<float> mMinMin;
		TypePlug<float> mMinMax;
		TypePlug<float> mMaxMin;
		TypePlug<float> mMaxMax;
		TypePlug<float> mValue;
		TypePlug<float> mResult;

		SmoothPulseFloat(): 
			mMinMin(true, CRCKey(eMinMin)),
			mMinMax(true, CRCKey(eMinMax)),
			mMaxMin(true, CRCKey(eMaxMin)),
			mMaxMax(true, CRCKey(eMaxMax)),
			mValue(true, CRCKey(eValue)),
			mResult(false, CRCKey(eResult))
		{
			mMinMin.m_Owner = this;
			mMinMax.m_Owner = this;
			mMaxMin.m_Owner = this;
			mMaxMax.m_Owner = this;
			mValue.m_Owner = this;
			mResult.m_Owner = this;
		}

		virtual ~SmoothPulseFloat(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 6;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mMinMin;
				case 1: return mMinMax;
				case 2: return mMaxMin;
				case 3: return mMaxMax;
				case 4: return mValue;
				case 5: 
				default: return mResult;
				
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mMinMin;
				case 1: return mMinMax;
				case 2: return mMaxMin;
				case 3: return mMaxMax;
				case 4: return mValue;
				case 5: 
				default: return mResult;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			float minmin, minmax, maxmin, maxmax, value;
			
			mMinMin.ReadData(&minmin, arEvaluationInfo);	
			mMinMax.ReadData(&minmax, arEvaluationInfo);	
			mMaxMin.ReadData(&maxmin, arEvaluationInfo);	
			mMaxMax.ReadData(&maxmax, arEvaluationInfo);	
			mValue.ReadData(&value, arEvaluationInfo);	

			float ret = math::smoothpulse( minmin, minmax, maxmin, maxmax, value);
			
			mResult.WriteData(&ret, arEvaluationInfo);
		}
	};

	class Mux5Float : public Node
	{
	public:
		static const eNodeType mId = Mux5FloatId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<float> mIn;
		TypePlug<float> mRangeA;
		TypePlug<float> mRangeB;
		TypePlug<float> mRangeC;
		TypePlug<float> mRangeD;
		TypePlug<float> mRangeE;
		TypePlug<float> mWeightA;
		TypePlug<float> mWeightB;
		TypePlug<float> mWeightC;
		TypePlug<float> mWeightD;
		TypePlug<float> mWeightE;

		Mux5Float() :	mIn(true,CRCKey(eIn)),
						mRangeA(true,CRCKey(eRangeA)),
						mRangeB(true,CRCKey(eRangeB)),
						mRangeC(true,CRCKey(eRangeC)),
						mRangeD(true,CRCKey(eRangeD)),
						mRangeE(true,CRCKey(eRangeE)),
						mWeightA(false,CRCKey(eWeightA)),
						mWeightB(false,CRCKey(eWeightB)),
						mWeightC(false,CRCKey(eWeightC)),
						mWeightD(false,CRCKey(eWeightD)),
						mWeightE(false,CRCKey(eWeightE))
		{
			mIn.m_Owner = this;
			mRangeA.m_Owner = this;
			mRangeB.m_Owner = this;
			mRangeC.m_Owner = this;
			mRangeD.m_Owner = this;
			mRangeE.m_Owner = this;
			mWeightA.m_Owner = this;
			mWeightB.m_Owner = this;
			mWeightC.m_Owner = this;
			mWeightD.m_Owner = this;
			mWeightE.m_Owner = this;
		}

		virtual ~Mux5Float(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 11;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mIn;
				case 1: return mRangeA;
				case 2: return mRangeB;
				case 3: return mRangeC;
				case 4: return mRangeD;
				case 5: return mRangeE;
				case 6: return mWeightA;
				case 7: return mWeightB;
				case 8: return mWeightC;
				case 9: return mWeightD;
				case 10: 
				default: return mWeightE;				
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mIn;
				case 1: return mRangeA;
				case 2: return mRangeB;
				case 3: return mRangeC;
				case 4: return mRangeD;
				case 5: return mRangeE;
				case 6: return mWeightA;
				case 7: return mWeightB;
				case 8: return mWeightC;
				case 9: return mWeightD;
				case 10: 
				default: return mWeightE;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			float in;
			float r[5];
			float w[5];
			
			mIn.ReadData(&in,arEvaluationInfo);	
			mRangeA.ReadData(&r[0],arEvaluationInfo);	
			mRangeB.ReadData(&r[1],arEvaluationInfo);	
			mRangeC.ReadData(&r[2],arEvaluationInfo);	
			mRangeD.ReadData(&r[3],arEvaluationInfo);	
			mRangeE.ReadData(&r[4],arEvaluationInfo);	
	
			w[0] = 1.0f;
			w[1] = 0.0f;
			w[2] = 0.0f;
			w[3] = 0.0f;
			w[4] = 0.0f;

			if(in > r[0])
			{
				w[0] = 0.f;
				for(uint32_t i = 0; i < 4; i++)
				{
					float d = r[i+1] - r[i];

					if(d > 0.0f)
					{
						if(in >= r[i] && in <= r[i+1])
						{
							w[i] = (d-(in-r[i]))/(r[i+1]-r[i]);
							w[i+1] = 1.f-w[i];
						}
					}
					else
					{
						break;
					}
				}
			}
				
			mWeightA.WriteData(&w[0],arEvaluationInfo);
			mWeightB.WriteData(&w[1],arEvaluationInfo);
			mWeightC.WriteData(&w[2],arEvaluationInfo);
			mWeightD.WriteData(&w[3],arEvaluationInfo);
			mWeightE.WriteData(&w[4],arEvaluationInfo);
		}
	};

	class Mul5Float : public Node
	{
	public:
		static const eNodeType mId = Mul5FloatId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<float> mIn;
		TypePlug<float> mA;
		TypePlug<float> mB;
		TypePlug<float> mC;
		TypePlug<float> mD;
		TypePlug<float> mE;
		TypePlug<float> mOutA;
		TypePlug<float> mOutB;
		TypePlug<float> mOutC;
		TypePlug<float> mOutD;
		TypePlug<float> mOutE;

		Mul5Float() :	mIn(true,CRCKey(eIn)),
						mA(true,CRCKey(eA)),
						mB(true,CRCKey(eB)),
						mC(true,CRCKey(eC)),
						mD(true,CRCKey(eD)),
						mE(true,CRCKey(eE)),
						mOutA(false,CRCKey(eOutA)),
						mOutB(false,CRCKey(eOutB)),
						mOutC(false,CRCKey(eOutC)),
						mOutD(false,CRCKey(eOutD)),
						mOutE(false,CRCKey(eOutE))
		{
			mIn.m_Owner = this;
			mOutA.m_Owner = this;
			mOutB.m_Owner = this;
			mOutC.m_Owner = this;
			mD.m_Owner = this;
			mE.m_Owner = this;
			mOutA.m_Owner = this;
			mOutB.m_Owner = this;
			mOutC.m_Owner = this;
			mOutD.m_Owner = this;
			mOutE.m_Owner = this;
		}

		virtual ~Mul5Float(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 11;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mIn;
				case 1: return mA;
				case 2: return mB;
				case 3: return mC;
				case 4: return mD;
				case 5: return mE;
				case 6: return mOutA;
				case 7: return mOutB;
				case 8: return mOutC;
				case 9: return mOutD;
				case 10: 
				default: return mOutE;				
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mIn;
				case 1: return mA;
				case 2: return mB;
				case 3: return mC;
				case 4: return mD;
				case 5: return mE;
				case 6: return mOutA;
				case 7: return mOutB;
				case 8: return mOutC;
				case 9: return mOutD;
				case 10: 
				default: return mOutE;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			float in;
			float i[5];
			float o[5];
			
			mIn.ReadData(&in,arEvaluationInfo);	
			mA.ReadData(&i[0],arEvaluationInfo);	
			mB.ReadData(&i[1],arEvaluationInfo);	
			mC.ReadData(&i[2],arEvaluationInfo);	
			mD.ReadData(&i[3],arEvaluationInfo);	
			mE.ReadData(&i[4],arEvaluationInfo);	
	
			for(uint32_t iter = 0; iter < 5; iter++)
			{
				o[iter] = i[iter] * in; 
			}
				
			mOutA.WriteData(&o[0],arEvaluationInfo);
			mOutB.WriteData(&o[1],arEvaluationInfo);
			mOutC.WriteData(&o[2],arEvaluationInfo);
			mOutD.WriteData(&o[3],arEvaluationInfo);
			mOutE.WriteData(&o[4],arEvaluationInfo);
		}
	};

	class Fmod : public Node
	{
	public:
		static const eNodeType mId = FmodId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<float> mNum;
		TypePlug<float> mDen;
		TypePlug<float> mRem;

		Fmod() : mNum(true,CRCKey(eNum)), mDen(true,CRCKey(eDen)), mRem(false, CRCKey(eRem))
		{
			mNum.m_Owner = this;
			mDen.m_Owner = this;
			mRem.m_Owner = this;
		}

		virtual ~Fmod(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 3;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mNum;
				case 1: return mDen;
				case 2:
				default: return mRem;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mNum;
				case 1: return mDen;
				case 2:
				default: return mRem;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			float n,d,r;
			
			mNum.ReadData(&n, arEvaluationInfo);	
			mDen.ReadData(&d, arEvaluationInfo);	

			r = math::fmod(n,d);

			mRem.WriteData(&r,arEvaluationInfo);
		}
	};

	class Sin : public Node
	{
	public:
		static const eNodeType mId = SinId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<float> mIn;
		TypePlug<float> mOut;

		Sin() : mIn(true,CRCKey(eIn)), mOut(false,CRCKey(eOut))
		{
			mIn.m_Owner = this;
			mOut.m_Owner = this;
		}

		virtual ~Sin(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 2;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mIn;
				case 1:
				default: return mOut;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mIn;
				case 1:
				default: return mOut;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			float i,o;
			
			mIn.ReadData(&i, arEvaluationInfo);	

			o = math::sin(i);

			mOut.WriteData(&o,arEvaluationInfo);
		}
	};

	class Rand : public Node
	{
	public:
		static const eNodeType mId = RandId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<float> mResult;

		Rand() : mResult(false,CRCKey(eResult))
		{
			mResult.m_Owner = this;
		}

		virtual ~Rand(){}

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
			float intpart   = static_cast<float>(std::rand());
			float fractpart = (std::rand() % 100) / 100.f;

			float result = intpart+fractpart;

			mResult.WriteData(&result,arEvaluationInfo);
		}
	};

	class Damp : public Node
	{
	public:
		static const eNodeType mId = DampId; 
		virtual eNodeType NodeType(){return mId;}

		TypePlug<float> mDampTime;
		TypePlug<float> mValue;
		TypePlug<float> mDeltaTime;
		TypePlug<float> mPreviousValue;
		TypePlug<float> mResult;

		Damp()
			:mDampTime(true,CRCKey(eDampTime)), 
			mValue(true,CRCKey(eValue)), 
			mDeltaTime(true,CRCKey(eDeltaTime)), 
			mPreviousValue(true,CRCKey(ePreviousValue)), 
			mResult(false,CRCKey(eResult))
		{
			mDampTime.m_Owner = this;
			mValue.m_Owner = this;
			mDeltaTime.m_Owner = this;
			mPreviousValue.m_Owner = this;
			mResult.m_Owner = this;
		}

		virtual ~Damp(){}

		virtual uint32_t GetPlugCount()const 
		{
			return 5;
		}
		virtual GraphPlug& GetPlug(uint32_t aIndex)
		{
			switch(aIndex)
			{
				case 0: return mDampTime;
				case 1: return mValue;
				case 2: return mDeltaTime;
				case 3: return mPreviousValue;
				case 4:
				default: return mResult;
			}
		}
		virtual GraphPlug const& GetPlug(uint32_t aIndex)const
		{
			switch(aIndex)
			{
				case 0: return mDampTime;
				case 1: return mValue;
				case 2: return mDeltaTime;
				case 3: return mPreviousValue;
				case 4:
				default: return mResult;
			}
		}

		virtual void Evaluate(EvaluationInfo& arEvaluationInfo)
		{
			float dampTime,value,deltaTime,previousValue, result;
			
			mDampTime.ReadData(&dampTime, arEvaluationInfo);	
			mValue.ReadData(&value, arEvaluationInfo);	
			mDeltaTime.ReadData(&deltaTime, arEvaluationInfo);	
			mPreviousValue.ReadData(&previousValue, arEvaluationInfo);	

			result = math::cond(dampTime > 0, previousValue + (value - previousValue) * deltaTime / (dampTime + deltaTime), value);

			mResult.WriteData(&result,arEvaluationInfo);
		}
	};

}

}
