#pragma once

// Must be kept in sync with AnimationClipStats in AnimationUtility.txt
struct AnimationClipStats
{
	int size;
	int positionCurves;
	int rotationCurves;
	int scaleCurves;
	int muscleCurves;
	int genericCurves;
	int pptrCurves;
	int totalCurves;
};