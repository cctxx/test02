#include "UnityPrefix.h"
#include "Runtime/mecanim/generic/stringtable.h"
#include "Runtime/mecanim/generic/crc32.h"

#define DEFINE_KEYWORD(s) table[mecanim::e##s] = mecanim::ReserveKeyword(mecanim::processCRC32(#s), #s)

namespace
{
	mecanim::ReserveKeyword* InitTable() 
	{
		static mecanim::ReserveKeyword table[mecanim::eLastString];

		DEFINE_KEYWORD(T);
		DEFINE_KEYWORD(Q);
		DEFINE_KEYWORD(S);
		DEFINE_KEYWORD(A);
		DEFINE_KEYWORD(B);
		DEFINE_KEYWORD(C);
		DEFINE_KEYWORD(D);
		DEFINE_KEYWORD(E);
		DEFINE_KEYWORD(X);
		DEFINE_KEYWORD(Y);
		DEFINE_KEYWORD(Z);
		DEFINE_KEYWORD(W);
		DEFINE_KEYWORD(Result);
		DEFINE_KEYWORD(Min);
		DEFINE_KEYWORD(Max);
		DEFINE_KEYWORD(Value);
		DEFINE_KEYWORD(MinMin);
		DEFINE_KEYWORD(MinMax);
		DEFINE_KEYWORD(MaxMin);
		DEFINE_KEYWORD(MaxMax);
		DEFINE_KEYWORD(In);
		DEFINE_KEYWORD(Out);
		DEFINE_KEYWORD(RangeA);
		DEFINE_KEYWORD(RangeB);
		DEFINE_KEYWORD(RangeC);
		DEFINE_KEYWORD(RangeD);
		DEFINE_KEYWORD(RangeE);
		DEFINE_KEYWORD(WeightA);
		DEFINE_KEYWORD(WeightB);
		DEFINE_KEYWORD(WeightC);
		DEFINE_KEYWORD(WeightD);
		DEFINE_KEYWORD(WeightE);
		DEFINE_KEYWORD(OutA);
		DEFINE_KEYWORD(OutB);
		DEFINE_KEYWORD(OutC);
		DEFINE_KEYWORD(OutD);
		DEFINE_KEYWORD(OutE);
		DEFINE_KEYWORD(Num);
		DEFINE_KEYWORD(Den); 
		DEFINE_KEYWORD(Rem);
		DEFINE_KEYWORD(DampTime); 
		DEFINE_KEYWORD(DeltaTime); 
		DEFINE_KEYWORD(PreviousValue);
		DEFINE_KEYWORD(GravityWeight); 
		DEFINE_KEYWORD(SrcRefX);
		DEFINE_KEYWORD(DstRefX);
		DEFINE_KEYWORD(SrcPivotX);
		DEFINE_KEYWORD(DstPivotX);
		DEFINE_KEYWORD(RefWeight);
		DEFINE_KEYWORD(PivotWeight);
		DEFINE_KEYWORD(XI);
		DEFINE_KEYWORD(XO);
		DEFINE_KEYWORD(Condition);
		DEFINE_KEYWORD(StateTime);
		DEFINE_KEYWORD(StateSpeed);		
		DEFINE_KEYWORD(StateExitTime);
		DEFINE_KEYWORD(DoTransition);
		DEFINE_KEYWORD(NextStateStartTime);
		DEFINE_KEYWORD(TransitionDuration);
		DEFINE_KEYWORD(TransitionOffset);		
		DEFINE_KEYWORD(TransitionStartTime);
		DEFINE_KEYWORD(StateMachineWeight);
		DEFINE_KEYWORD(TransitionTime);
		DEFINE_KEYWORD(BlendWeight);
		DEFINE_KEYWORD(StateWeight);
		DEFINE_KEYWORD(StabilizeFeet);
		DEFINE_KEYWORD(RootX);		
		table[mecanim::eLeftFootWeightT]			= mecanim::ReserveKeyword(mecanim::processCRC32("LeftFoot.WeightT"), "LeftFoot.WeightT");
		table[mecanim::eLeftFootWeightR]			= mecanim::ReserveKeyword(mecanim::processCRC32("LeftFoot.WeightR"), "LeftFoot.WeightR");
		table[mecanim::eRightFootWeightT]			= mecanim::ReserveKeyword(mecanim::processCRC32("RightFoot.WeightT"), "RightFoot.WeightT");
		table[mecanim::eRightFootWeightR]			= mecanim::ReserveKeyword(mecanim::processCRC32("RightFoot.WeightR"), "RightFoot.WeightR");
		DEFINE_KEYWORD(ComputeSource);
		DEFINE_KEYWORD(LookAt);
		DEFINE_KEYWORD(LeftFootX);
		DEFINE_KEYWORD(RightFootX);
		DEFINE_KEYWORD(LeftFootSpeedT);
		DEFINE_KEYWORD(LeftFootSpeedQ);
		DEFINE_KEYWORD(RightFootSpeedT);
		DEFINE_KEYWORD(RightFootSpeedQ);
		DEFINE_KEYWORD(LeftFootStableT);
		DEFINE_KEYWORD(LeftFootStableQ);
		DEFINE_KEYWORD(RightFootStableT);
		DEFINE_KEYWORD(RightFootStableQ);
		DEFINE_KEYWORD(RootSpeedT);
		DEFINE_KEYWORD(RootSpeedQ);
		DEFINE_KEYWORD(RootStableT);
		DEFINE_KEYWORD(RootStableQ);
		DEFINE_KEYWORD(LeftFootProjX);
		DEFINE_KEYWORD(RightFootProjX);
		DEFINE_KEYWORD(PlantFeet);
		DEFINE_KEYWORD(LeftFootSafeX);
		DEFINE_KEYWORD(RightFootSafeX);		
		DEFINE_KEYWORD(PositionX);
		DEFINE_KEYWORD(PositionY);
		DEFINE_KEYWORD(PositionZ);
		DEFINE_KEYWORD(QuaternionX);
		DEFINE_KEYWORD(QuaternionY);
		DEFINE_KEYWORD(QuaternionZ);
		DEFINE_KEYWORD(QuaternionW);
		DEFINE_KEYWORD(ScaleX);
		DEFINE_KEYWORD(ScaleY);
		DEFINE_KEYWORD(ScaleZ);
		DEFINE_KEYWORD(DynamicCurve);
		return table;
	}
}

namespace mecanim
{
	ReserveKeyword* ReserveKeywordTable()
	{
		static ReserveKeyword* s_Table = InitTable();
		return s_Table;
	}
	uint32_t CRCKey(eString id)
	{
		return ReserveKeywordTable()[id].m_ID;
	}
	}
