#pragma once


struct BoundCurveDeprecated
{
	int     targetInstanceID;
	
	// The ptr to the data we are writing into
	void*   targetPtr; 
	
	// type of the data we will write into kBindFloatValue, kBindQuaternion etc.
	// - Materials:  4 bits targetType, 24 bits shader property name, 4 bits sub property index (x, y, z, w)
	// - SkinnedMeshRenderer: 4 bits targetType, the rest BlendShape weight index
	UInt32  targetType;
	
	// Which states affect the bound curve?
	UInt32  affectedStateMask; 
	
	Object* targetObject;
	
	BoundCurveDeprecated () { targetObject = NULL; targetType = 0; targetInstanceID = 0; targetPtr = 0; affectedStateMask = 0; }
	
	enum
	{
		kBindTypeBitCount = 4,
		kBindMaterialShaderPropertyNameBitCount = 24,

		kBindTypeMask = (1 << kBindTypeBitCount) - 1,
		kBindMaterialShaderPropertyNameMask = ((1 << kBindMaterialShaderPropertyNameBitCount) - 1) << kBindTypeBitCount,
		kBindMaterialShaderSubpropertyMask = ~(kBindTypeMask | kBindMaterialShaderPropertyNameMask),
	};
};

enum
{
	kUnbound = 0,
	kBindTransformPosition = 1, // This enum may not be changed. It is used in GenericClipBinding.
	kBindTransformRotation = 2, // This enum may not be changed.  It is used in GenericClipBinding.
	kBindTransformScale = 3,    //  It is used in GenericClipBinding.
	kMinGenericBinding = 4,

	kBindFloat,
	kBindFloatToBool,
	kBindFloatToBlendShapeWeight,
	kBindFloatToGameObjectActivate,
	
	kMinIntCurveBinding,
	kBindMaterialPPtrToRenderer = kMinIntCurveBinding,
#if ENABLE_SPRITES
	kBindSpritePPtrToSpriteRenderer,
#endif
	kMaxIntCurveBinding,

	// These must always come last since materials do special bit masking magic.
	kBindFloatToMaterial = kMaxIntCurveBinding,
	kBindFloatToColorMaterial,
	kBindFloatToMaterialScaleAndOffset,
	
	kBindTypeCount
};
