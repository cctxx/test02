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

namespace mecanim
{

namespace graph
{
	class quatIdentityOp
	{
	public:
		template<typename RESULT> static math::float4 Operation(){ return math::quatIdentity(); } 
	};

	class quatIdentity : public ResultNode<math::float4, quatIdentityOp> 
	{
	public: 
		static const eNodeType mId = quatIdentityId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE, typename RESULT> class quatConjOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quatConj(l); } 
	};

	class quatConj : public UnaryNode<math::float4, math::float4, quatConjOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quatConjId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class quatMulOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::float4 Operation( math::float4 const& l, math::float4 const& r){ return math::quatMul(l, r); } 
	};

	class quatMul : public BinaryNode<math::float4, math::float4, math::float4, quatMulOp> 
	{
	public: 
		static const eNodeType mId = quatMulId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class quatMulVecOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::float4 Operation( math::float4 const& l, math::float4 const& r){ return math::quatMulVec(l, r); } 
	};

	class quatMulVec : public BinaryNode<math::float4, math::float4, math::float4, quatMulVecOp> 
	{
	public: 
		static const eNodeType mId = quatMulVecId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class quatLerpOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename TYPE3, typename RESULT> static math::float4 Operation( math::float4 const& a, math::float4 const& b, float c){ return math::quatLerp(a, b, math::float1(c)); } 
	};

	class quatLerp : public TernaryNode<math::float4, math::float4, float, math::float4, quatLerpOp> 
	{
	public: 
		static const eNodeType mId = quatLerpId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class quatArcRotateOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::float4 Operation( math::float4 const& l, math::float4 const& r){ return math::quatArcRotate(l, r); } 
	};

	class quatArcRotate : public BinaryNode<math::float4, math::float4, math::float4, quatArcRotateOp> 
	{
	public: 
		static const eNodeType mId = quatArcRotateId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class quatArcRotateXOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quatArcRotateX(l); } 
	};

	class quatArcRotateX : public UnaryNode<math::float4, math::float4, quatArcRotateXOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quatArcRotateXId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class quatXcosOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quatXcos(l); } 
	};

	class quatXcos : public UnaryNode<math::float4, math::float4, quatXcosOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quatXcosId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class quatYcosOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quatYcos(l); } 
	};

	class quatYcos : public UnaryNode<math::float4, math::float4, quatYcosOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quatYcosId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class quatZcosOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quatZcos(l); } 
	};

	class quatZcos : public UnaryNode<math::float4, math::float4, quatZcosOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quatZcosId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class quatEulerToQuatOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quatEulerToQuat(l); } 
	};

	class quatEulerToQuat : public UnaryNode<math::float4, math::float4, quatEulerToQuatOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quatEulerToQuatId; 
		virtual eNodeType NodeType(){return mId;}
	};
	template<typename TYPE1, typename RESULT> class quatQuatToEulerOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quatQuatToEuler(l); } 
	};

	class quatQuatToEuler : public UnaryNode<math::float4, math::float4, quatQuatToEulerOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quatQuatToEulerId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class quatProjOnYPlaneOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quatProjOnYPlane(l); } 
	};

	class quatProjOnYPlane : public UnaryNode<math::float4, math::float4, quatProjOnYPlaneOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quatProjOnYPlaneId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class quat2QtanOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quat2Qtan(l); } 
	};

	class quat2Qtan : public UnaryNode<math::float4, math::float4, quat2QtanOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quat2QtanId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class qtan2QuatOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::qtan2Quat(l); } 
	};

	class qtan2Quat : public UnaryNode<math::float4, math::float4, qtan2QuatOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = qtan2QuatId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class ZYRoll2QuatOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::ZYRoll2Quat(l); } 
	};

	class ZYRoll2Quat : public UnaryNode<math::float4, math::float4, ZYRoll2QuatOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = ZYRoll2QuatId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class quat2ZYRollOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quat2ZYRoll(l); } 
	};

	class quat2ZYRoll : public UnaryNode<math::float4, math::float4, quat2ZYRollOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quat2ZYRollId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class RollZY2QuatOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::RollZY2Quat(l); } 
	};

	class RollZY2Quat : public UnaryNode<math::float4, math::float4, RollZY2QuatOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = RollZY2QuatId; 
		virtual eNodeType NodeType(){return mId;}
	};

	template<typename TYPE1, typename RESULT> class quat2RollZYOp
	{
	public:
		static math::float4 Operation( math::float4 const& l){ return math::quat2RollZY(l); } 
	};

	class quat2RollZY : public UnaryNode<math::float4, math::float4, quat2RollZYOp<math::float4, math::float4> > 
	{
	public: 
		static const eNodeType mId = quat2RollZYId; 
		virtual eNodeType NodeType(){return mId;}
	};

	class quatWeightOp
	{
	public:
		template<typename TYPE1, typename TYPE2, typename RESULT> static math::float4 Operation( math::float4 const& l, float r){ return math::quatWeight(l, math::float1(r)); } 
	};

	class quatWeight : public BinaryNode<math::float4, float, math::float4, quatWeightOp> 
	{
	public: 
		static const eNodeType mId = quatWeightId; 
		virtual eNodeType NodeType(){return mId;}
	};	
}
}