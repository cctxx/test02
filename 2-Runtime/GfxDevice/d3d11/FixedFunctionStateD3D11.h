#pragma once

#include "Runtime/GfxDevice/GfxDeviceTypes.h"


enum TextureSourceD3D11 {
	kTexSourceUV0,
	kTexSourceUV1,
	kTexSourceUV2,
	kTexSourceUV3,
	kTexSourceUV4,
	kTexSourceUV5,
	kTexSourceUV6,
	kTexSourceUV7,
	// match the order of TexGenMode!
	kTexSourceSphereMap,
	kTexSourceObject,
	kTexSourceEyeLinear,
	kTexSourceCubeReflect,
	kTexSourceCubeNormal,
	kTexSourceTypeCount
};

#define CMP_STATE(member) { \
	if (a.member < b.member) \
		return true; \
	else if (b.member < a.member) \
		return false; \
	}


struct FixedFunctionStateD3D11
{
	FixedFunctionStateD3D11()
	: texUnitSources(0)
	, texUnitCube(0)
	, texUnit3D(0)
	, texUnitProjected(0)
	, texUnitCount(0)
	, alphaTest(kFuncDisabled)
	, useUniformInsteadOfVertexColor(false)
	, lightingEnabled(false)
	, specularEnabled(false)
	, lightCount(0)
	, colorMaterial(kColorMatDisabled)
	, fogMode(kFogDisabled)
	{
		for (int i = 0; i < kMaxSupportedTextureUnits; ++i)
		{
			texUnitColorCombiner[i] = ~0U;
			texUnitAlphaCombiner[i] = ~0U;
		}
	}
	
	UInt64	texUnitSources;	// 4 bits for each unit
	UInt32	texUnitColorCombiner[kMaxSupportedTextureUnits];
	UInt32	texUnitAlphaCombiner[kMaxSupportedTextureUnits];
	UInt32	texUnitCube; // bit per unit
	UInt32	texUnit3D; // bit per unit
	UInt32	texUnitProjected; // bit per unit
	
	int		texUnitCount;
	CompareFunction alphaTest;
	
	bool	useUniformInsteadOfVertexColor;
	bool	lightingEnabled;
	bool	specularEnabled;
	int		lightCount;
	ColorMaterialMode colorMaterial;
	FogMode	fogMode;
};


struct FixedFuncStateCompareD3D11
{
	bool operator() (const FixedFunctionStateD3D11& a, const FixedFunctionStateD3D11& b) const
	{
		CMP_STATE(lightingEnabled);
		CMP_STATE(specularEnabled);
		CMP_STATE(lightCount);
		CMP_STATE(texUnitCount);
		CMP_STATE(texUnitSources);
		CMP_STATE(texUnitCube);
		CMP_STATE(texUnit3D);
		CMP_STATE(texUnitProjected);
		CMP_STATE(alphaTest);
		for (int i = 0; i < a.texUnitCount; i++)
		{
			CMP_STATE(texUnitColorCombiner[i])
			CMP_STATE(texUnitAlphaCombiner[i])
		}
		CMP_STATE(useUniformInsteadOfVertexColor);
		CMP_STATE(colorMaterial);
		CMP_STATE(fogMode);
		
		return false;
	}
};
