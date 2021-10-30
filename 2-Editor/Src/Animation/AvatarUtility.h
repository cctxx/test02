#ifndef AVATARUTILITY_H
#define AVATARUTILITY_H

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Math/Quaternion.h"

#include "Runtime/Animation/MecanimUtility.h"
#include "Runtime/Animation/AvatarBuilder.h"


#include <string>
#include <vector>
#include <map>

class Transform;
class HumanTemplate;
class Avatar;
class Animator;

namespace Unity { class GameObject; }

struct TransformX
{
    Vector3f	position;
    Quaternionf rotation;
    Vector3f	scale;
};

class AvatarUtility
{
public:

		// The type of animation to support / import
	enum AnimationType
	{
		kNoAnimationType = 0,
		kLegacy          = 1,
		kGeneric         = 2,
		kHumanoid        = 3
	};

	static void HumanGetColliderTransform(Avatar& avatar, int boneIndex, TransformX const& bone, TransformX& collider);
	static void HumanSubColliderTransform(Avatar& avatar, int boneIndex, TransformX const& bone, TransformX& collider);

	static void SetHumanPose(Animator& animator, float* dof, int dofCount);
protected:
};

#endif
