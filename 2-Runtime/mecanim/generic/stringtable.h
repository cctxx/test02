#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/types.h"


namespace mecanim
{
	enum eString
	{
		eT,
		eQ,
		eS,
		eA,
		eB,
		eC,
		eD,
		eE,
		eX,
		eY,
		eZ,
		eW,
		eResult,
		eMin,
		eMax,
		eValue,
		eMinMin,
		eMinMax,
		eMaxMin,
		eMaxMax,
		eIn,
		eOut,
		eRangeA,
		eRangeB,
		eRangeC,
		eRangeD,
		eRangeE,
		eWeightA,
		eWeightB,
		eWeightC,
		eWeightD,
		eWeightE,
		eOutA,
		eOutB,
		eOutC,
		eOutD,
		eOutE,
		eNum,
		eDen, 
		eRem,
		eDampTime,
		eDeltaTime,
		ePreviousValue,
		eGravityWeight,
		eSrcRefX,
		eDstRefX,
		eSrcPivotX,
		eDstPivotX,
		eRefWeight,
		ePivotWeight,
		eXI,
		eXO,
		eCondition,
		eStateTime,
		eStateSpeed,		
		eStateExitTime,
		eDoTransition,
		eNextStateStartTime,
		eTransitionDuration,
		eTransitionOffset,		
		eTransitionStartTime,
		eStateMachineWeight,	
		eTransitionTime,
		eBlendWeight,
		eStateWeight,
		eStabilizeFeet,
		eRootX,		
		eLeftFootWeightT,
		eLeftFootWeightR,
		eRightFootWeightT,
		eRightFootWeightR,
		eComputeSource,
		eLookAt,
		eLeftFootX,
		eRightFootX,
		eLeftFootSpeedT,
		eLeftFootSpeedQ,
		eRightFootSpeedT,
		eRightFootSpeedQ,
		eLeftFootStableT,
		eLeftFootStableQ,
		eRightFootStableT,
		eRightFootStableQ,
		eRootSpeedT,
		eRootSpeedQ,
		eRootStableT,
		eRootStableQ,
		eLeftFootProjX,
		eRightFootProjX,
		ePlantFeet,
		eLeftFootSafeX,
		eRightFootSafeX,
		ePositionX,
		ePositionY,
		ePositionZ,
		eQuaternionX,
		eQuaternionY,
		eQuaternionZ,
		eQuaternionW,
		eScaleX,
		eScaleY,
		eScaleZ,
		eDynamicCurve,
		eLastString
	};

	struct ReserveKeyword 
	{
		ReserveKeyword():m_ID(0),m_Keyword(0){}
		ReserveKeyword(uint32_t id, char const* keyword):m_ID(id),m_Keyword(keyword){}

		uint32_t	m_ID;
		char const*	m_Keyword ;
	};

	ReserveKeyword* ReserveKeywordTable();
	uint32_t CRCKey(eString id);
}
