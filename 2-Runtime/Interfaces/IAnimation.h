#pragma once

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Utilities/NonCopyable.h"
#include "Runtime/Modules/ExportModules.h"
#include "Runtime/Math/Matrix4x4.h"

typedef UInt32 BindingHash;

#include "Runtime/Math/Matrix4x4.h"

namespace Unity {
	class Component;
}

struct CalculateSkinMatricesTask
{
	// input
	const void*									skeletonPose;
	const UInt16*								skeletonIndices;
	Matrix4x4f									rootPose;
	int											bindPoseCount;
	const Matrix4x4f*							bindPose;
	// output
	Matrix4x4f*									outPose;
};

typedef void* (*CalculateAnimatorSkinMatricesFunc)(void* userData);

class EXPORT_COREMODULE IAnimation : public NonCopyable
{
public:
	virtual const void* GetGlobalSpaceSkeletonPose(const Unity::Component& animator) = 0;

	virtual bool CalculateWorldSpaceMatricesMainThread(Unity::Component& animator, const UInt16* indices, size_t count, Matrix4x4f* outMatrices) = 0;

	virtual CalculateAnimatorSkinMatricesFunc GetCalculateAnimatorSkinMatricesFunc() = 0;

	virtual bool PathHashesToIndices(Unity::Component& animator, const BindingHash* bonePathHashes, size_t count, UInt16* outIndices) = 0;
};

EXPORT_COREMODULE IAnimation* GetAnimationInterface();
EXPORT_COREMODULE void SetAnimationInterface(IAnimation* theInterface);
