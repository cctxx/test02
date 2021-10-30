#pragma once

#include "Runtime/Misc/BuildSettings.h"

struct AnimationClipSettings
{
	float	m_StartTime;
	float	m_StopTime;
	float	m_OrientationOffsetY;
	float	m_Level;
	float	m_CycleOffset;

	bool	m_LoopTime;
	bool	m_LoopBlend;
	bool	m_LoopBlendOrientation;
	bool	m_LoopBlendPositionY;
	bool	m_LoopBlendPositionXZ;
	bool	m_KeepOriginalOrientation;
	bool	m_KeepOriginalPositionY;
	bool	m_KeepOriginalPositionXZ;
	bool	m_HeightFromFeet;
	bool	m_Mirror;

	DEFINE_GET_TYPESTRING (MuscleClipInfo)

	AnimationClipSettings()
	{
		m_StartTime = 0;
		m_StopTime = 1;
		m_OrientationOffsetY = 0;
		m_Level = 0;
		m_CycleOffset = 0;

		m_LoopTime = false;
		m_LoopBlend = false;
		m_LoopBlendOrientation = false;
		m_LoopBlendPositionY = false;
		m_LoopBlendPositionXZ = false;
		m_KeepOriginalOrientation = false;
		m_KeepOriginalPositionY = true;
		m_KeepOriginalPositionXZ = false;
		m_HeightFromFeet = false;
		m_Mirror = false;
	}

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer)
	{
		transfer.SetVersion(2);

		TRANSFER(m_StartTime);
		TRANSFER(m_StopTime);
		TRANSFER(m_OrientationOffsetY);
		TRANSFER(m_Level);
		TRANSFER(m_CycleOffset);

		TRANSFER(m_LoopTime);
		TRANSFER(m_LoopBlend);
		
		TRANSFER(m_LoopBlendOrientation);
		TRANSFER(m_LoopBlendPositionY);
		TRANSFER(m_LoopBlendPositionXZ);
		TRANSFER(m_KeepOriginalOrientation);
		TRANSFER(m_KeepOriginalPositionY);
		TRANSFER(m_KeepOriginalPositionXZ);
		TRANSFER(m_HeightFromFeet);
		TRANSFER(m_Mirror);
		transfer.Align();
		
		// Backwards compatibility (4.3 introduced seperate loopTime / loopBlend)
		if (transfer.IsVersionSmallerOrEqual(1))
			m_LoopTime = m_LoopBlend;
	}
};

