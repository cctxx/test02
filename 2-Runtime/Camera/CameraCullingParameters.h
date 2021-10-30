#pragma once

#include "ShaderReplaceData.h"
#include "Runtime/Utilities/EnumFlags.h"
#include "UnityPrefix.h"
#include "Camera.h"
#include "External/shaderlab/Library/shaderlab.h"


namespace Umbra { class DebugRenderer; }

enum CullFlag
{
	kCullFlagForceEvenIfCameraIsNotActive	= 1 << 0,
	kCullFlagOcclusionCull					= 1 << 1,
	kCullFlagNeedsLighting					= 1 << 2,
};

struct CameraCullingParameters
{
	Camera*               cullingCamera;
	ShaderReplaceData     explicitShaderReplace;
	CullFlag              cullFlag;
	Umbra::DebugRenderer* umbraDebugRenderer;
	UInt32                umbraDebugFlags;
	
	CameraCullingParameters (Camera& cam, CullFlag flag)
	{ 
		cullingCamera = &cam; 
		cullFlag = flag; 
		umbraDebugRenderer = NULL; 
		umbraDebugFlags = 0;
	}
};

ENUM_FLAGS(CullFlag);
