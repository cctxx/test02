#pragma once

class IAnimationBinding;

namespace UnityEngine
{
namespace Animation
{
		

// These values can can never be changed, when a system is deprecated,
// it must be kept commented out and the index is not to be reused!
// The enum value can not be reused. Otherwise built data (assetbundles) will break.
enum BindType
{
	kUnbound							= 0,
	
	// Builtin transform bindings
	kBindTransformPosition				= 1, // This enum may not be changed. It is used in GenericClipBinding.
	kBindTransformRotation				= 2, // This enum may not be changed.  It is used in GenericClipBinding.
	kBindTransformScale					= 3, // It is used in GenericClipBinding.
	
	// Builtin float bindings
	kMinSinglePropertyBinding			= 5,
	kBindFloat							= 5,
	kBindFloatToBool					= 6,
	kBindGameObjectActive				= 7,
	kBindMuscle							= 8,

	
	// Custom bindings
	kBlendShapeWeightBinding			= 20,
	kRendererMaterialPPtrBinding		= 21,
	kRendererMaterialPropertyBinding	= 22,
	kSpriteRendererPPtrBinding			= 23,
	kMonoBehaviourPropertyBinding		= 24,
	kAllBindingCount
};

struct BoundCurve
{
	void*				targetPtr; 
	UInt32				targetType;
	IAnimationBinding*	customBinding;
	Object*				targetObject;
	
	BoundCurve () { targetObject = 0; targetPtr = 0; targetType = 0; customBinding = 0; }
};
	
}
}
