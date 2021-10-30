#pragma once

#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/GfxDevice/ShaderConstantCache.h"
#include "Runtime/GfxDevice/TransformState.h"
#include "D3D9Includes.h"

class BuiltinShaderParamValues;

enum TextureSourceMode {
	kTexSourceUV0,
	kTexSourceUV1,
	// match the order of TexGenMode!
	kTexSourceSphereMap,
	kTexSourceObject,
	kTexSourceEyeLinear,
	kTexSourceCubeReflect,
	kTexSourceCubeNormal,
	kTexSourceTypeCount
};

enum TextureMatrixMode {
	kTexMatrixNone,
	kTexMatrix2,
	kTexMatrix3,
	kTexMatrix4,
	kTexMatrixTypeCount
};

struct VertexPipeConfig {
	// 2 bytes
	UInt64 textureMatrixModes : 16;	// TextureMatrixMode: 2 bits for each unit
	// 3 bytes
	UInt64 textureSources : 24;		// TextureSourceMode: 3 bits for each unit
	// 1 byte
	UInt64 colorMaterial : 3;		// ColorMaterialMode
	UInt64 texCoordCount : 4;		// number of texture coordinates
	UInt64 hasVertexColor : 1;		// is vertex color coming from per-vertex data?
	// 1 byte
	UInt64 hasLighting : 1;			// lighting on?
	UInt64 hasSpecular : 1;			// specular on?
	UInt64 hasLightType : 3;		// has light of given type? (bit per type)
	UInt64 hasNormalization : 1;	// needs to normalize normals?
	// 10 bits left

	void Reset() {
		memset(this, 0, sizeof(*this));
	}

	void SetTextureUnit( UInt32 unit ) {
		Assert (unit < 8);
		UInt32 tc = texCoordCount;
		if( unit >= tc ) {
			tc = unit+1;
			texCoordCount = tc;
		}
	}
	void ClearTextureUnit( UInt32 unit ) {
		Assert (unit < 8);
		UInt32 tc = texCoordCount;
		if( unit < tc ) {
			tc = unit;
			texCoordCount = tc;
		}
	}
};


struct VertexPipeDataD3D9 
{
	GfxVertexLight	lights[kMaxSupportedVertexLights];
	D3DMATERIAL9	material;
	SimpleVec4		ambient;
	SimpleVec4		ambientClamped;
	int				vertexLightCount;
	UInt32			projectedTextures;	// 1 bit per unit


	NormalizationMode	normalization;

	mutable bool			haveToResetDeviceState;

	void Reset() {
		memset (&material, 0, sizeof(material));
		ambient.set (0,0,0,0);
		ambientClamped.set (0,0,0,0);
		vertexLightCount = 0;
		projectedTextures = 0;
		normalization = kNormalizationUnknown;
		haveToResetDeviceState = false;
	}
};


struct VertexPipePrevious {
	VertexPipeConfig config;
	SimpleVec4	ambient;
	int			vertexLightCount;
	IDirect3DVertexShader9* vertexShader;

	void Reset() {
		config.Reset ();
		ambient.set(-1,-1,-1,-1);
		vertexLightCount = 0;
		vertexShader = NULL;
	}
};

void ResetVertexPipeStateD3D9 (
	IDirect3DDevice9* dev,
	TransformState& state,
	BuiltinShaderParamValues& builtins,
	VertexPipeConfig& config,
	VertexPipeDataD3D9& data,
	VertexPipePrevious& previous);

void SetupFixedFunctionD3D9 (
	IDirect3DDevice9* dev,
	TransformState& state,
	BuiltinShaderParamValues& builtins,
	const VertexPipeConfig& config,
	const VertexPipeDataD3D9& data,
	VertexPipePrevious& previous,
	bool vsActive, bool immediateMode);

void SetupVertexShaderD3D9 (
	IDirect3DDevice9* dev,
	TransformState& state,
	const BuiltinShaderParamValues& builtins,
	VertexPipeConfig& config,
	const VertexPipeDataD3D9& data,
	VertexPipePrevious& previous,
	VertexShaderConstantCache& cache,
	bool vsActive, bool immediateMode);

void CleanupVertexShadersD3D9 ();
