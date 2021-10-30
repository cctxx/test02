#pragma once

#include <limits>
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/mecanim/generic/valuearray.h"
#include "Runtime/mecanim/skeleton/skeleton.h"
#include "Runtime/mecanim/animation/avatar.h"

typedef UInt32 BindingHash;

namespace Unity { class GameObject; }

namespace UnityEngine
{
namespace Animation
{
	struct AnimationClipBindingConstant;
	struct AnimationSetBindings;
	struct BoundCurve;
	
	
	struct ExposedTransform
	{
		Transform*			transform;
		int					skeletonIndex;
		int					skeletonIndexForUpdateTransform;
	};

	struct AvatarBindingConstant
	{
		// For non-optimized mode (the Transform hierarchy is there)
		size_t        skeletonBindingsCount;
		Transform**   skeletonBindings;

		int           transformChangedMask;

		// For optimized mode
		size_t				exposedTransformCount;
		ExposedTransform*	exposedTransforms;
	};

	struct AnimatorGenericBindingConstant
	{
		size_t				transformBindingsCount;
		BoundCurve*			transformBindings;

		size_t				genericBindingsCount;
		BoundCurve*			genericBindings;

		size_t				genericPPtrBindingsCount;
		BoundCurve*			genericPPtrBindings;

		int					transformChangedMask;
		
		mecanim::animation::ControllerBindingConstant*	controllerBindingConstant;
		
		bool				allowConstantClipSamplingOptimization;
	};
	
	AvatarBindingConstant* CreateAvatarBindingConstant (Transform& root, const mecanim::animation::AvatarConstant* avatar, mecanim::memory::Allocator& allocator);
	AvatarBindingConstant* CreateAvatarBindingConstantOpt (Transform& root, const mecanim::animation::AvatarConstant* avatar, mecanim::memory::Allocator& allocator);
	void DestroyAvatarBindingConstant (AvatarBindingConstant* bindingConstant, mecanim::memory::Allocator& allocator);

	/// Bind multiple animation clips against a GameObject
	/// outputCurveIndexToBindingIndex is an array of arrays that remaps from the curve index of each clip into the bound curves array
	/// returns a binding constant.
	AnimatorGenericBindingConstant* CreateAnimatorGenericBindings (const AnimationSetBindings& animationSet, Transform& root, const mecanim::animation::AvatarConstant* avatar, const mecanim::animation::ControllerConstant* controller, mecanim::memory::Allocator& allocator);
	AnimatorGenericBindingConstant* CreateAnimatorGenericBindingsOpt (const AnimationSetBindings& animationSet, Transform& root, const mecanim::animation::AvatarConstant* avatar, const mecanim::animation::ControllerConstant* controller, mecanim::memory::Allocator& allocator);
	void DestroyAnimatorGenericBindings (AnimatorGenericBindingConstant* bindingConstant, mecanim::memory::Allocator& allocator);

	/// Batch assign an array of values to the bound locations (Also calls AwakeFromLoad)
	/// Must always be called from the main thread
	void SetGenericFloatPropertyValues (const AnimatorGenericBindingConstant& bindings, const mecanim::ValueArray& values);
	void SetGenericPPtrPropertyValues   (const AnimatorGenericBindingConstant& bindings, const mecanim::ValueArray& values);

	/// Batch assign transform properties to the bound Transform components
	/// Can be invoked from another thread if we are sure no one will delete Transforms at the same time on another thread.
	void SetGenericTransformPropertyValues (const AnimatorGenericBindingConstant& bindings, const mecanim::ValueArray& values, Transform *skipTransform);
	void SetHumanTransformPropertyValues (const AvatarBindingConstant& bindings, const mecanim::skeleton::SkeletonPose& pose);
	
	/// Invoke TransformChanged callbacks and SetDirty
	/// Must always be called from the main thread
	void SetTransformPropertyApplyMainThread (Transform& root, const AnimatorGenericBindingConstant& bindings, const AvatarBindingConstant& avatarBindings, bool skipRoot);
	void SetTransformPropertyApplyMainThread (Transform& root, const AvatarBindingConstant& avatarBindings, bool skipRoot, int mask = std::numeric_limits<int>::max() );

	/// Set transforms for optimized characters
	/// Must always be called from the main thread
	void SetFlattenedSkeletonTransformsMainThread (const AvatarBindingConstant& bindings, const mecanim::skeleton::SkeletonPose& globalSpacePose, const mecanim::animation::AvatarConstant& avatar);

	void UnregisterAvatarBindingObjects(AvatarBindingConstant* bindingConstant);
	void UnregisterGenericBindingObjects(AnimatorGenericBindingConstant* bindingConstant);

}
}
