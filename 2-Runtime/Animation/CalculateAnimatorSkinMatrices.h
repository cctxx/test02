#pragma once

#include "Runtime/Interfaces/IAnimation.h"

class Animator;

// Multi-threaded.
// Can only get animated pose.
// Bindpose included.
void*	DoCalculateAnimatorSkinMatrices (void* userData);

// Main thread.
// Can handle both cases: animated pose & default pose.
// Bindpose not included.
bool	CalculateWordSpaceMatrices (Animator* animator, const UInt16* skeletonIndices, Matrix4x4f* outWorldSpaceMatrices, size_t size);



