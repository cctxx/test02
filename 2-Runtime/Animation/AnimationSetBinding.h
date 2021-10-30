#pragma once

#include "AnimationClipBindings.h"
#include "Runtime/BaseClasses/BaseObject.h"

class RuntimeAnimatorController;

class AnimationClip;

namespace mecanim { namespace animation { struct AnimationSet; struct ControllerConstant; } }
namespace mecanim { namespace memory    { class Allocator;    } }
typedef std::vector<PPtr<AnimationClip> > AnimationClipVector;

namespace UnityEngine
{
namespace Animation
{


struct TransformBinding
{
	BindingHash    path;
	int            bindType;
};
	
struct AnimationSetBindings
{
	size_t                 genericBindingsSize;                      
	GenericBinding*        genericBindings;

	size_t                 genericPPtrBindingsSize;                      
	GenericBinding*        genericPPtrBindings;
	
	size_t                 transformBindingsNonConstantSize;
	size_t                 transformBindingsSize;                      
	TransformBinding*      transformBindings;
	
	
	// See ConstantCurveOptimization below:
	size_t                 constantCurveValueCount;
	float*                 constantCurveValues;
	
	mecanim::animation::AnimationSet*               animationSet;
};

AnimationSetBindings* CreateAnimationSetBindings (mecanim::animation::ControllerConstant const* controller, AnimationClipVector const& clips, mecanim::memory::Allocator& allocator);
void DestroyAnimationSetBindings (AnimationSetBindings* bindings, mecanim::memory::Allocator& allocator);
size_t GetCurveCountForBindingType (UInt32 targetType);
	
	
/* 
    *** Constant Curve optimization overview ***

We found that animationclips for generic characters in most cases contain a lot of scale curves and often also translation curves which are completely constant.
This has a massive negative impact on performance. StreamedClip sampling is negatively affected because all the coefficients need to be evaluated.
But more importantly the ValueArray used for blending becomes very big. Especially since blending scale curves is pretty expensive (log scale blend)
 
It would have been easy to simply remove the curves that are constant when importing the clips,
but there is no good automatic default for it because in some cases the user actually makes constant curves intentionally to get an effect when blending between clips.

So when creating the mecanim clip we classify all curves as constant curves and streamedclip curves. Constant Curves are put at the end.

Constantclip evaluation is trivial, simply a memcpy.
 
When binding we also detect if all the clips have the same constant values. If they do we put the constant bindings at the end of the ValueArrayConstant.
On the instance we then need to verify if the constant curves on the clip matches the default values on the instance.
This is because defaultValues are used for blending. If they are used for blending and different between defaults and clip values, then they must be represented in the ValueArray.

Thus on the instance we have a subset of the value array and on the instance we can simply reduce the value array by cutting of the end of the ValueArray.
All data is laid out to be able to do this in the AnimationSetBinding.
 
*/	
}
}
