#pragma once

class AnimationClip;
class Vector2f;

struct MuscleClipQualityInfo
{
	float m_Loop;
	float m_LoopOrientation;
	float m_LoopPositionY; 
	float m_LoopPositionXZ;

	MuscleClipQualityInfo()
	{
		m_Loop = 0;
		m_LoopOrientation = 0;
		m_LoopPositionY = 0; 
		m_LoopPositionXZ = 0;
	}
};

MuscleClipQualityInfo	GetMuscleClipQualityInfo(AnimationClip& clip, float startTime, float stopTime);
void					CalculateQualityCurves (AnimationClip& clip, float fixedTime, float variableEndStart, float variableEndEnd, int q, int sampleCount, Vector2f* poseCurve, Vector2f* rotationCurve, Vector2f* heightCurve, Vector2f* positionCurve);
