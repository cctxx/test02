#pragma once

#include "Runtime/Interfaces/IAnimation.h"


class MecanimAnimation : public IAnimation
{
public:
	static void InitializeClass ();
	static void CleanupClass ();

	MecanimAnimation() {}
	~MecanimAnimation() {}

	virtual const void* GetGlobalSpaceSkeletonPose(const Unity::Component& animator);

	virtual bool CalculateWorldSpaceMatricesMainThread(Unity::Component& animator, const UInt16* indices, size_t count, Matrix4x4f* outMatrices);

	virtual CalculateAnimatorSkinMatricesFunc GetCalculateAnimatorSkinMatricesFunc();

	virtual bool PathHashesToIndices(Unity::Component& animator, const BindingHash* bonePathHashes, size_t count, UInt16* outIndices);

};