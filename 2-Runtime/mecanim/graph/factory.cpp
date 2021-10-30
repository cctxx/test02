#include "UnityPrefix.h"
#include "Runtime/mecanim/graph/factory.h"

#include "Runtime/mecanim/graph/unarynode.h"
#include "Runtime/mecanim/graph/binarynode.h"
#include "Runtime/mecanim/graph/genericnode.h"
#include "Runtime/mecanim/graph/xformnode.h"
#include "Runtime/mecanim/graph/quaternionnode.h"

#define REGISTERNODE(classnode)					\
	case classnode::mId:						\
	{											\
		node = arAlloc.Construct<classnode>();	\
		break;									\
	}

namespace mecanim
{

namespace graph
{
	Node* GraphFactory::Create(eNodeType aNodeId, memory::Allocator& arAlloc)const
	{
		Node* node = 0;
		switch(aNodeId)
		{
			REGISTERNODE(NegationFloat);
			REGISTERNODE(NegationInt);
			REGISTERNODE(NegationFloat4);
			REGISTERNODE(NegationBool);			
			REGISTERNODE(AdditionFloat);
			REGISTERNODE(AdditionUInt);
			REGISTERNODE(AdditionInt);
			REGISTERNODE(AdditionFloat4);
			REGISTERNODE(SubstractionFloat);
			REGISTERNODE(SubstractionUInt);
			REGISTERNODE(SubstractionInt);
			REGISTERNODE(SubstractionFloat4);
			REGISTERNODE(MultiplicationFloat);
			REGISTERNODE(MultiplicationUInt);
			REGISTERNODE(MultiplicationInt);
			REGISTERNODE(MultiplicationFloat4);
			REGISTERNODE(DivisionFloat);
			REGISTERNODE(DivisionUInt);
			REGISTERNODE(DivisionInt);
			REGISTERNODE(DivisionFloat4);
			REGISTERNODE(CondFloat);
			REGISTERNODE(CondUInt);
			REGISTERNODE(CondInt);
			REGISTERNODE(CondFloat4);
			REGISTERNODE(AbsFloat);
			REGISTERNODE(AbsFloat4);
			REGISTERNODE(CrossFloat4);
			REGISTERNODE(DegreesFloat);
			REGISTERNODE(DegreesFloat4);
			REGISTERNODE(DotFloat4);
			REGISTERNODE(LengthFloat4);
			REGISTERNODE(MaximumFloat);
			REGISTERNODE(MaximumUInt);
			REGISTERNODE(MaximumInt);
			REGISTERNODE(MaximumFloat4);
			REGISTERNODE(MinimumFloat);
			REGISTERNODE(MinimumUInt);
			REGISTERNODE(MinimumInt);
			REGISTERNODE(MinimumFloat4);
			REGISTERNODE(NormalizeFloat4);
			REGISTERNODE(RadiansFloat);
			REGISTERNODE(RadiansFloat4);
			REGISTERNODE(FloatToFloat4);
			REGISTERNODE(Float4ToFloat);
			REGISTERNODE(GreaterThan);
			REGISTERNODE(LesserThan);
			REGISTERNODE(GreaterThanOrEqual);
			REGISTERNODE(LesserThanOrEqual);
			REGISTERNODE(SmoothStepFloat);
			REGISTERNODE(Mux5Float);
			REGISTERNODE(Mul5Float);
			REGISTERNODE(Sin);
			REGISTERNODE(Fmod);
			REGISTERNODE(And);
			REGISTERNODE(Or);
			REGISTERNODE(xformMulInv);
			REGISTERNODE(xformIdentity);
			REGISTERNODE(xformMulVec);
			REGISTERNODE(xformInvMulVec);
			REGISTERNODE(xformMul);
			REGISTERNODE(xformInvMul);
			REGISTERNODE(xformEqual);
			REGISTERNODE(xformWeight);
			REGISTERNODE(xformAdd);
			REGISTERNODE(xformSub);
			REGISTERNODE(xformBlend);
			REGISTERNODE(quatIdentity);
			REGISTERNODE(quatConj);
			REGISTERNODE(quatMul);
			REGISTERNODE(quatMulVec);
			REGISTERNODE(quatLerp);
			REGISTERNODE(quatArcRotate);
			REGISTERNODE(quatArcRotateX);
			REGISTERNODE(quatXcos);
			REGISTERNODE(quatYcos);
			REGISTERNODE(quatZcos);
			REGISTERNODE(quatEulerToQuat);
			REGISTERNODE(quatQuatToEuler);
			REGISTERNODE(quatProjOnYPlane);
			REGISTERNODE(quat2Qtan);
			REGISTERNODE(qtan2Quat);
			REGISTERNODE(ZYRoll2Quat);
			REGISTERNODE(quat2ZYRoll);
			REGISTERNODE(RollZY2Quat);
			REGISTERNODE(quat2RollZY);
			REGISTERNODE(quatWeight);
			REGISTERNODE(xformCompose);
			REGISTERNODE(xformDecompose);
			REGISTERNODE(CondXform);
			REGISTERNODE(Rand);
			REGISTERNODE(Damp);
			REGISTERNODE(xformRefChange);
			REGISTERNODE(Xor);
			REGISTERNODE(SmoothPulseFloat);
		}
		return node;
	}

	GraphPlug* GraphFactory::Create(ePlugType aPlugType, memory::Allocator& arAlloc)const
	{
		GraphPlug* plug = 0;
		switch(aPlugType)
		{
			case Float4Id:
			{
				plug =  arAlloc.Construct< TypePlug<math::float4> >();
				break;
			}
			case Float1Id:
			case FloatId:
			{
				plug =  arAlloc.Construct< TypePlug<float> >();
				break;
			}
			case UInt32Id:
			{
				plug =  arAlloc.Construct< TypePlug<uint32_t> >();
				break;
			}
			case Int32Id:
			{
				plug =  arAlloc.Construct< TypePlug<int32_t> >();
				break;
			}
			case BoolId:
			{
				plug =  arAlloc.Construct< TypePlug<bool> >();
				break;
			}
			case Bool4Id:
			{
				plug =  arAlloc.Construct< TypePlug<math::bool4> >();
				break;
			}
			case XformId:
			{
				plug =  arAlloc.Construct< TypePlug<math::xform> >();
				break;
			}
			
		}
		return plug;
	}
}

}
