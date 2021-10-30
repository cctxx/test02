#pragma once

struct RootMotionData
{
	Vector3f    deltaPosition;
	Quaternionf targetRotation;
	float       gravityWeight;
	bool        didApply;
};